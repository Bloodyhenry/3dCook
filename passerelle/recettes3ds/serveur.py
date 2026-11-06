"""Serveur HTTP simple (sans TLS) pour le réseau local.

Pour la 3DS :
  GET  /menu                    menu (liste courte + rev)
  GET  /recipes/<id>            une recette au format 3DS
  GET  /recipes/<id>/image      photo JPEG ≤ 400×240 (404 si aucune)
Pour la page web (téléphone) :
  GET  /                        page du menu
  GET  /api/info                source utilisée et ce que la page peut faire
  GET  /api/recherche?q=…       recherche (source export JSON seulement)
  PUT  /api/menu/<id>           ajoute au menu       (idem)
  DELETE /api/menu/<id>         retire du menu       (idem)
  PUT  /api/corrections/<id>    {"portions": {"nombre": 6, "unite": "ramequins"} | null}

Avec Mealie, le menu vient du planning : les routes d'écriture répondent 405.
Si la source ne répond pas, la passerelle répond 502 et la 3DS garde son cache.
Pas d'authentification : à réserver au réseau local.
"""

import json
import re
import socket
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from .sources import Source, SourceIndisponible

_RE_ID = re.compile(r"[A-Za-z0-9_-]{1,32}")
_PAGE = Path(__file__).with_name("web") / "index.html"
_TAILLE_CORPS_MAX = 4096


def adresse_locale() -> str | None:
    """Adresse IP de cette machine sur le réseau local (sans rien envoyer)."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("10.255.255.255", 1))   # UDP : aucun paquet n'est émis
            return s.getsockname()[0]
    except OSError:
        return None


def adresse_pour_3ds(hote_http: str | None, port: int) -> str | None:
    """Adresse à saisir sur la console.

    On reprend l'adresse utilisée par le navigateur (« 192.168.1.10:8383 »),
    sauf si c'est localhost : on donne alors l'IP locale de la machine.
    """
    if hote_http:
        nom = hote_http.rsplit(":", 1)[0] if not hote_http.startswith("[") else hote_http
        if nom not in ("localhost", "127.0.0.1", "[::1]"):
            return f"http://{hote_http}"
    ip = adresse_locale()
    return f"http://{ip}:{port}" if ip else None


def _json_octets(donnees) -> bytes:
    return json.dumps(donnees, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


class Gestionnaire(BaseHTTPRequestHandler):
    server_version = "recettes3ds/1"
    # Fixé par creer_serveur().
    source: Source

    # --- Réponses -----------------------------------------------------------------

    def _envoyer(self, code: int, corps: bytes, type_contenu: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", type_contenu)
        self.send_header("Content-Length", str(len(corps)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(corps)

    def _json(self, donnees, code: int = 200) -> None:
        self._envoyer(code, _json_octets(donnees), "application/json; charset=utf-8")

    def _erreur(self, code: int, message: str) -> None:
        self._json({"erreur": message}, code)

    def log_message(self, fmt, *args):
        sys.stderr.write(f"{self.address_string()} {fmt % args}\n")

    # --- Routage ----------------------------------------------------------------------

    def _route(self) -> tuple[list[str], dict]:
        url = urlparse(self.path)
        return [m for m in url.path.split("/") if m], parse_qs(url.query)

    def _traiter(self, fonction) -> None:
        """Appelle la fonction de route en traduisant les erreurs en codes HTTP."""
        try:
            fonction()
        except SourceIndisponible as e:
            self._erreur(502, str(e))
        except ValueError as e:     # JSON invalide, paramètre invalide…
            self._erreur(400, str(e) or "requête invalide")

    def _recette_ou_404(self, rid: str) -> dict | None:
        if not _RE_ID.fullmatch(rid):
            self._erreur(400, "identifiant invalide")
            return None
        r = self.source.recette(rid)
        if r is None:
            self._erreur(404, "recette inconnue")
        return r

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        self._traiter(self._get)

    def do_PUT(self):
        self._traiter(self._put)

    def do_DELETE(self):
        self._traiter(self._delete)

    def _get(self):
        s = self.source
        chemin, params = self._route()
        if chemin == []:
            self._envoyer(200, _PAGE.read_bytes(), "text/html; charset=utf-8")
        elif chemin == ["menu"]:
            self._json(s.menu())
        elif chemin == ["api", "info"]:
            info = s.info()
            info["adresse_3ds"] = adresse_pour_3ds(self.headers.get("Host"),
                                                   self.server.server_address[1])
            self._json(info)
        elif len(chemin) == 2 and chemin[0] == "recipes":
            r = self._recette_ou_404(chemin[1])
            if r is not None:
                self._json(r)
        elif len(chemin) == 3 and chemin[0] == "recipes" and chemin[2] == "image":
            if self._recette_ou_404(chemin[1]) is None:
                return
            image = s.chemin_image(chemin[1])
            if image is None:
                self._erreur(404, "pas de photo")
            else:
                self._envoyer(200, image.read_bytes(), "image/jpeg")
        elif chemin == ["api", "recherche"]:
            if not s.recherche:
                self._erreur(405, f"la recherche se fait dans {s.nom}")
                return
            q = params.get("q", [""])[0]
            limite = min(int(params.get("limite", ["50"])[0] or 50), 400)
            self._json(s.rechercher(q, limite))
        else:
            self._erreur(404, "chemin inconnu")

    def _lire_corps(self) -> dict:
        taille = int(self.headers.get("Content-Length") or 0)
        if taille > _TAILLE_CORPS_MAX:
            raise ValueError("corps trop gros")
        corps = json.loads((self.rfile.read(taille) if taille else b"{}").decode("utf-8"))
        if not isinstance(corps, dict):
            raise ValueError("objet JSON attendu")
        return corps

    def _put(self):
        s = self.source
        chemin, _ = self._route()
        if len(chemin) != 3 or chemin[0] != "api" or chemin[1] not in ("menu", "corrections"):
            self._erreur(404, "chemin inconnu")
            return
        if chemin[1] == "menu" and not s.menu_modifiable:
            self._erreur(405, f"le menu se gère dans {s.nom}")
            return
        if chemin[1] == "corrections" and not s.corrections:
            self._erreur(405, f"les recettes se corrigent dans {s.nom}")
            return
        rid = chemin[2]
        if self._recette_ou_404(rid) is None:
            return
        if chemin[1] == "menu":
            s.ajouter(rid)
            self._json(s.menu())
            return
        portions = _valider_portions(self._lire_corps().get("portions"))
        s.definir_portions(rid, portions)
        self._json(s.recette(rid))

    def _delete(self):
        s = self.source
        chemin, _ = self._route()
        if len(chemin) != 3 or chemin[:2] != ["api", "menu"]:
            self._erreur(404, "chemin inconnu")
        elif not s.menu_modifiable:
            self._erreur(405, f"le menu se gère dans {s.nom}")
        elif not _RE_ID.fullmatch(chemin[2]):
            self._erreur(400, "identifiant invalide")
        else:
            s.retirer(chemin[2])   # même si la recette a disparu de la base
            self._json(s.menu())


def _valider_portions(p) -> dict | None:
    """{"nombre": 6, "unite": "ramequins"} validé, ou None pour l'auto."""
    if p is None:
        return None
    if not isinstance(p, dict):
        raise ValueError("portions invalides")
    nombre = p.get("nombre")
    unite = str(p.get("unite") or "portions").strip()[:30]
    if not isinstance(nombre, int) or isinstance(nombre, bool) or not 1 <= nombre <= 200:
        raise ValueError("le nombre de portions doit être un entier entre 1 et 200")
    return {"nombre": nombre, "unite": unite}


def creer_serveur(source: Source, hote: str, port: int) -> ThreadingHTTPServer:
    gestionnaire = type("GestionnaireLie", (Gestionnaire,), {"source": source})
    serveur = ThreadingHTTPServer((hote, port), gestionnaire)
    serveur.daemon_threads = True
    return serveur
