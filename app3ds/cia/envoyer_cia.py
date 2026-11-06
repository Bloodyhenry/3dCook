"""Installe recettes.cia sur la console par Wi-Fi, avec FBI.

    python3 cia/envoyer_cia.py 192.168.1.67 recettes.cia

Sur la console, ouvrir FBI, puis « Remote Install », puis « Receive URLs over
the network ». FBI attend alors une liste d'adresses sur le port 5000.

Ce script :
  1. sert le fichier .cia en HTTP depuis le Mac (port 8484) ;
  2. envoie son adresse à FBI (longueur sur 4 octets gros-boutiste, puis les
     adresses séparées par des retours à la ligne) ;
  3. attend que FBI réponde (un octet, une fois l'installation terminée),
     puis arrête le serveur.
Rien n'est écrit sur le Mac ; seul le fichier .cia est servi.
"""

import functools
import http.server
import socket
import struct
import sys
import threading
from pathlib import Path

PORT_FBI = 5000
PORT_HTTP = 8484


def adresse_locale(vers: str) -> str:
    """IP du Mac sur le réseau de la console (sans rien envoyer)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect((vers, PORT_FBI))
        return s.getsockname()[0]


class Fichier(http.server.SimpleHTTPRequestHandler):
    """Ne sert que le fichier .cia demandé, rien d'autre du dossier."""

    nom = ""

    def do_GET(self):
        if self.path.lstrip("/") != self.nom:
            self.send_error(404)
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        sys.stderr.write(f"  FBI télécharge : {fmt % args}\n")


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit("usage : envoyer_cia.py <ip-de-la-3ds> <fichier.cia>")
    console, cia = sys.argv[1], Path(sys.argv[2]).resolve()
    if not cia.is_file():
        sys.exit(f"{cia} introuvable : lancer d'abord « make cia ».")

    mac = adresse_locale(console)
    gestionnaire = functools.partial(type("F", (Fichier,), {"nom": cia.name}),
                                     directory=str(cia.parent))
    serveur = http.server.ThreadingHTTPServer(("", PORT_HTTP), gestionnaire)
    threading.Thread(target=serveur.serve_forever, daemon=True).start()
    url = f"http://{mac}:{PORT_HTTP}/{cia.name}"
    print(f"Fichier servi : {url}")

    try:
        with socket.create_connection((console, PORT_FBI), timeout=10) as s:
            charge = url.encode("ascii")
            s.sendall(struct.pack("!L", len(charge)) + charge)
            print("Adresse envoyée à FBI : confirme l'installation sur la console…")
            s.settimeout(600)
            if not s.recv(1):
                print("FBI a fermé la connexion.")
            else:
                print("Installation terminée.")
    except OSError as e:
        sys.exit(f"Impossible de joindre FBI sur {console}:{PORT_FBI} ({e}). "
                 "Sur la console : FBI → Remote Install → Receive URLs over the network.")
    finally:
        serveur.shutdown()


if __name__ == "__main__":
    main()
