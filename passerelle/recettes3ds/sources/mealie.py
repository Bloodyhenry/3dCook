"""Source « Mealie » : lit les recettes et le planning de repas par l'API REST.

La passerelle ne fait que des requêtes GET : elle ne modifie jamais Mealie.
Elle a besoin d'un jeton d'API (Mealie → Profil → Jetons d'API).

Correspondances (Mealie v3) :
  - menu de la 3DS      ← planning de repas des N prochains jours ;
  - ingrédients         ← recipeIngredient : structurés (quantité / unité /
                          aliment) ou texte libre, analysé alors en français ;
  - minuteurs           ← durée saisie sur l'étape (durationSeconds), sinon
                          durées trouvées dans le texte de l'étape ;
  - portions            ← recipeYieldQuantity + recipeYield, sinon recipeServings ;
  - allergènes          ← étiquettes « Allergène : … » ;
  - congelable          ← étiquette « Congelable ».
Les identifiants 3DS sont les UUID Mealie sans tirets (32 caractères).
"""

import datetime as dt
import json
import re
import threading
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path

from .. import allergenes
from ..construire import construire
from ..etapes import nettoyer_etape
from ..images import image_depuis_octets
from ..ingredients import _UNITES, analyser_ligne
from ..texte import normaliser, sans_accents
from . import Source, SourceIndisponible

REPAS = {
    "breakfast": "Petit-déjeuner",
    "lunch": "Déjeuner",
    "snack": "En-cas",
    "dinner": "Dîner",
    "side": "Accompagnement",
    "dessert": "Dessert",
    "drink": "Boisson",
}
_ORDRE_REPAS = list(REPAS)

_RE_UNITE_EXACTE = re.compile(
    "|".join(rf"(?P<u{i}>(?:{motif}))" for i, (motif, _) in enumerate(_UNITES)), re.IGNORECASE)


# --- Conversion d'une recette Mealie → JSON 3DS -----------------------------------

def texte_markdown(texte: str | None) -> str:
    """Texte Markdown/HTML de Mealie → une ligne de texte simple."""
    if not texte:
        return ""
    t = re.sub(r"<[^>]+>", " ", texte)
    t = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", t)          # liens et images
    t = re.sub(r"(?m)^\s*(?:#{1,6}\s+|>\s*|[-*+]\s+)", "", t)    # titres, citations, puces
    t = t.replace("**", "").replace("__", "").replace("`", "")
    return normaliser(t)


def unite_canonique(unite: dict | None) -> str | None:
    """Unité Mealie → forme utilisée par la 3DS (« Gramme » → « g »)."""
    if not unite:
        return None
    candidats = [unite.get("abbreviation"), unite.get("name"), unite.get("pluralName")]
    for c in candidats:
        if c and _RE_UNITE_EXACTE.fullmatch(c.strip()):
            m = _RE_UNITE_EXACTE.fullmatch(c.strip())
            return _UNITES[int(m.lastgroup[1:])][1]
    if unite.get("useAbbreviation") and unite.get("abbreviation"):
        return unite["abbreviation"].strip()
    return (unite.get("name") or "").strip() or None


def _nombre_json(v):
    if v is None:
        return None
    v = round(float(v), 3)
    return int(v) if v == int(v) else v


def ingredient_mealie(ing: dict, allergenes_recette: set[str]) -> dict | None:
    """Un ingrédient Mealie → item 3DS (None s'il est vide)."""
    aliment = ing.get("food") or None
    unite = ing.get("unit") or None
    note = texte_markdown(ing.get("note"))
    quantite = ing.get("quantity") or None

    if aliment or unite:
        # Ingrédient structuré par Mealie : on reprend ses champs.
        nom = (aliment or {}).get("name", "").strip() if aliment else ""
        if quantite and quantite >= 2 and aliment and aliment.get("pluralName"):
            nom = aliment["pluralName"].strip()
        nom = ", ".join(x for x in (nom, note) if x) or texte_markdown(ing.get("display"))
        item = {
            "quantite": _nombre_json(quantite),
            "quantite_max": None,
            "unite": unite_canonique(unite),
            "nom": nom,
            "allergene": False,
        }
    else:
        # Texte libre (cas des recettes importées) : analyse française.
        texte = note or texte_markdown(ing.get("display")) or texte_markdown(ing.get("originalText"))
        if not texte:
            return None
        item = analyser_ligne(texte)

    if not item["nom"]:
        return None
    texte_complet = " ".join(str(x) for x in (item["unite"], item["nom"]) if x)
    item["allergene"] = bool(allergenes_recette and allergenes.detecter(texte_complet, allergenes_recette))
    return item


def portions_mealie(r: dict) -> dict | None:
    quantite = r.get("recipeYieldQuantity") or 0
    texte = (r.get("recipeYield") or "").strip()
    if quantite > 0:
        return {"nombre": max(1, round(quantite)), "unite": texte or "portions"}
    m = re.match(r"^(\d{1,3})\s+(.+)$", texte)
    if m:
        return {"nombre": int(m[1]), "unite": m[2].strip()}
    if (r.get("recipeServings") or 0) > 0:
        return {"nombre": max(1, round(r["recipeServings"])), "unite": "portions"}
    return None


def convertir_mealie(r: dict, rid: str, image: bool) -> dict:
    """Recette complète de l'API Mealie (GET /api/recipes/{id}) → JSON 3DS."""
    noms_tags = [t.get("name", "") for t in r.get("tags") or []]
    allergenes_recette = allergenes.depuis_tags(noms_tags)

    groupes = [{"titre": None, "items": []}]
    for ing in r.get("recipeIngredient") or []:
        titre = texte_markdown(ing.get("title"))
        if titre:
            if groupes[-1]["items"]:
                groupes.append({"titre": titre, "items": []})
            else:
                groupes[-1]["titre"] = titre
        item = ingredient_mealie(ing, allergenes_recette)
        if item:
            groupes[-1]["items"].append(item)
    groupes = [g for g in groupes if g["items"]]

    etapes = []
    for pas in r.get("recipeInstructions") or []:
        texte = nettoyer_etape(texte_markdown(pas.get("text")))
        if not texte:
            continue
        etape = {"texte": texte}
        section = texte_markdown(pas.get("title")) or texte_markdown(pas.get("summary"))
        if section:
            etape["section"] = section
        duree = pas.get("durationSeconds")
        if isinstance(duree, int) and duree > 0:
            etape["secondes"] = duree
        etapes.append(etape)

    notes = []
    for n in r.get("notes") or []:
        texte = texte_markdown(n.get("text"))
        titre = texte_markdown(n.get("title"))
        if texte:
            notes.append(f"{titre} : {texte}" if titre else texte)

    return construire(
        rid=rid,
        titre=normaliser(r.get("name") or "Sans titre"),
        portions=portions_mealie(r),
        congelable=any(sans_accents(t).strip() == "congelable" for t in noms_tags),
        image=image,
        groupes=groupes,
        etapes=etapes,
        notes=notes,
        conseil=texte_markdown(r.get("description")) or None,
    )


# --- Accès HTTP --------------------------------------------------------------------

class ClientMealie:
    """Client minimal de l'API Mealie (GET uniquement)."""

    def __init__(self, url: str, jeton: str, delai: float = 10.0):
        self.url = url.rstrip("/")
        self.jeton = jeton
        self.delai = delai

    def _ouvrir(self, chemin: str, params: dict | None = None) -> bytes | None:
        adresse = self.url + chemin
        if params:
            adresse += "?" + urllib.parse.urlencode(params)
        req = urllib.request.Request(adresse, headers={
            "Authorization": f"Bearer {self.jeton}",
            "Accept": "application/json",
        })
        try:
            with urllib.request.urlopen(req, timeout=self.delai) as rep:
                return rep.read()
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            if e.code in (401, 403):
                raise SourceIndisponible("Mealie refuse le jeton d'API") from e
            raise SourceIndisponible(f"Mealie a répondu {e.code}") from e
        except (urllib.error.URLError, TimeoutError, OSError) as e:
            raise SourceIndisponible(f"Mealie injoignable : {e}") from e

    def json(self, chemin: str, params: dict | None = None):
        brut = self._ouvrir(chemin, params)
        return None if brut is None else json.loads(brut.decode("utf-8"))

    def octets(self, chemin: str) -> bytes | None:
        return self._ouvrir(chemin)


def _uuid(rid: str) -> str | None:
    """« 9f3c…(32 hex) » → UUID canonique, ou None si l'id n'en est pas un."""
    try:
        return str(uuid.UUID(hex=rid))
    except ValueError:
        return None


def _epoch(horodatage: str | None) -> int:
    if not horodatage:
        return 0
    try:
        return int(dt.datetime.fromisoformat(horodatage.replace("Z", "+00:00")).timestamp())
    except ValueError:
        return 0


class SourceMealie(Source):
    nom = "Mealie"

    def __init__(self, url: str, jeton: str, dossier_data: Path, jours: int = 7,
                 lien: str | None = None, client: ClientMealie | None = None,
                 aujourd_hui=dt.date.today):
        self.client = client or ClientMealie(url, jeton)
        self.dossier_cache = Path(dossier_data) / "cache" / "images"
        self.jours = max(1, jours)
        self.lien = lien
        self._aujourd_hui = aujourd_hui
        self._verrou = threading.Lock()
        # id 3DS → (clé de version Mealie, recette 3DS, clé d'image)
        self._cache: dict[str, tuple[str, dict, str | None]] = {}

    @staticmethod
    def _cle_version(r: dict) -> str:
        return str(r.get("updatedAt") or r.get("dateUpdated") or "")

    def _charger(self, rid: str) -> tuple[str, dict, str | None] | None:
        identifiant = _uuid(rid)
        if identifiant is None:
            return None
        brute = self.client.json(f"/api/recipes/{identifiant}")
        if brute is None:
            return None
        cle_image = brute.get("image") or None
        entree = (self._cle_version(brute), convertir_mealie(brute, rid, bool(cle_image)),
                  str(cle_image) if cle_image else None)
        with self._verrou:
            self._cache[rid] = entree
        return entree

    def recette(self, rid: str) -> dict | None:
        entree = self._charger(rid)
        return entree[1] if entree else None

    def _recette_a_jour(self, rid: str, cle_version: str) -> tuple[str, dict, str | None] | None:
        """Recette depuis le cache si Mealie indique qu'elle n'a pas changé."""
        with self._verrou:
            entree = self._cache.get(rid)
        if entree and cle_version and entree[0] == cle_version:
            return entree
        return self._charger(rid)

    def chemin_image(self, rid: str) -> Path | None:
        with self._verrou:
            entree = self._cache.get(rid)
        if entree is None:
            entree = self._charger(rid)
        if entree is None or entree[2] is None:
            return None
        cle = re.sub(r"[^A-Za-z0-9_-]", "", entree[2])[:32]
        destination = self.dossier_cache / f"{rid}-{cle}.jpg"
        if destination.is_file():
            return destination
        octets = self.client.octets(f"/api/media/recipes/{_uuid(rid)}/images/original.webp")
        if not octets:
            return None
        return image_depuis_octets(octets, destination)

    def menu(self) -> dict:
        debut = self._aujourd_hui()
        fin = debut + dt.timedelta(days=self.jours - 1)
        page = self.client.json("/api/households/mealplans", {
            "start_date": debut.isoformat(),
            "end_date": fin.isoformat(),
            "include_week_entries": "true",
            "page": 1,
            "perPage": 200, "per_page": 200,
            "orderBy": "date", "orderDirection": "asc",
        }) or {}

        entrees = [e for e in page.get("items", []) if e.get("recipeId") and e.get("recipe")]

        def ordre(e):
            type_repas = e.get("entryType", "")
            rang = _ORDRE_REPAS.index(type_repas) if type_repas in _ORDRE_REPAS else len(_ORDRE_REPAS)
            return (e.get("date", ""), rang)

        recettes, vus, maj = [], set(), 0
        for e in sorted(entrees, key=ordre):
            rid = uuid.UUID(str(e["recipeId"])).hex
            if rid in vus:
                continue
            vus.add(rid)
            resume = e["recipe"]
            entree = self._recette_a_jour(rid, self._cle_version(resume))
            if entree is None:
                continue
            r = entree[1]
            maj = max(maj, _epoch(e.get("updatedAt")), _epoch(self._cle_version(resume)))
            recettes.append({
                "id": rid,
                "titre": r["titre"],
                "rev": r["rev"],
                "image": r["image"],
                "date": e.get("date"),
                "repas": REPAS.get(e.get("entryType"), e.get("entryType") or ""),
                "echelle": e.get("recipeScale") or 1,
            })
        return {"v": 1, "maj": maj, "recettes": recettes}
