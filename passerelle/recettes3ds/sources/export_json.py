"""Source « export JSON » : un export statique de recettes (fichier JSON dont
les champs title, ingredientsHtml et prepHtml contiennent du HTML), en lecture seule.

Contient aussi la conversion vers schema.org (format Nextcloud Cookbook) qui
sert à importer ces recettes dans Mealie : voir outils/exporter_mealie.py.

Le fichier JSON est relu automatiquement quand il change (nouvel export).
Rien n'est jamais écrit dans le dossier source.
"""

import json
import os
import threading
from pathlib import Path
from urllib.parse import urlparse

from .. import allergenes
from ..construire import construire
from ..etapes import analyser_etapes
from ..etat import Etat
from ..images import image_en_cache
from ..ingredients import analyser_ingredients, detecter_portions, est_titre_groupe
from ..texte import html_vers_lignes, html_vers_texte, normaliser, sans_accents, segments_rouges
from . import Source

# Nom du fichier JSON par défaut dans le dossier source.
NOM_JSON = "recettes.json"


def nom_fichier_image(source: dict) -> str | None:
    """Nom du fichier local de la photo (« acras.jpg »), d'après son URL."""
    url = source.get("image") or ""
    nom = os.path.basename(urlparse(url).path)
    return nom or None


def analyser(source: dict) -> dict:
    """Découpe une recette brute ; résultat commun au JSON 3DS et à schema.org."""
    titre = normaliser(html_vers_texte(source.get("title", "")))
    lignes_ingr = html_vers_lignes(source.get("ingredientsHtml"))
    lignes_prep = html_vers_lignes(source.get("prepHtml"))
    etapes, notes = analyser_etapes(lignes_prep)
    portions = detecter_portions(
        "\n".join(t for t, _ in lignes_ingr),
        titre,
        "\n".join(t for t, _ in lignes_prep),
    )
    rouges = segments_rouges(source.get("ingredientsHtml"))
    trouves = set()
    non_reconnus = []
    for segment in rouges:
        a = allergenes.detecter(segment)
        trouves |= a
        if not a:
            non_reconnus.append(segment)
    return {
        "titre": titre,
        "lignes_ingr": lignes_ingr,
        "etapes": etapes,
        "notes": notes,
        "portions": portions,
        "conseil": html_vers_texte(source.get("conseil")) or None,
        "congelable": bool(source.get("hasCongelation")),
        "allergenes": sorted(trouves, key=allergenes.NOMS.index),
        "rouges_non_reconnus": non_reconnus,
    }


def convertir_export(source: dict, image_presente: bool = False,
                     correction: dict | None = None) -> dict:
    """Recette brute (export JSON) → JSON 3DS.

    `correction` vient de corrections.json (ex. {"portions": {...}}) et
    prime sur la détection automatique.
    """
    a = analyser(source)
    portions = correction["portions"] if correction and "portions" in correction else a["portions"]
    return construire(
        rid=str(source["id"]),
        titre=a["titre"],
        portions=portions,
        congelable=a["congelable"],
        image=image_presente,
        groupes=analyser_ingredients(a["lignes_ingr"]),
        etapes=a["etapes"],
        notes=a["notes"],
        conseil=a["conseil"],
    )


def _instructions_schemaorg(etapes: list[dict]) -> list[dict]:
    """Étapes → HowToStep, regroupées en HowToSection quand elles ont une section."""
    sortie: list[dict] = []
    for e in etapes:
        pas = {"@type": "HowToStep", "text": e["texte"]}
        if e.get("section"):
            sortie.append({"@type": "HowToSection", "name": e["section"], "itemListElement": [pas]})
        elif sortie and sortie[-1]["@type"] == "HowToSection":
            # Les étapes suivantes restent dans la section ouverte.
            sortie[-1]["itemListElement"].append(pas)
        else:
            sortie.append(pas)
    return sortie


def vers_schemaorg(source: dict, nom: str | None = None, etiquette: str | None = None) -> dict:
    """Recette brute → schema.org Recipe (JSON-LD), prête pour l'import Mealie.

    Deux extensions de Mealie, ignorées par les autres logiciels :
      - recipeIngredient contient des objets {"note", "title"} pour garder les
        titres de groupe (« Pour la pâte ») ;
      - notes : liste de textes (conservation, astuces).
    Les allergènes et la congélation deviennent des mots-clés, que Mealie
    transforme en étiquettes. `etiquette` (facultative) est ajoutée en tête
    pour retrouver les recettes importées.
    """
    a = analyser(source)
    ingredients = []
    titre_groupe = None
    for texte, _ in a["lignes_ingr"]:
        texte = texte.lstrip("-*•· ").strip()
        if est_titre_groupe(texte):
            titre_groupe = texte.rstrip(" :")
            continue
        ingredient = {"note": texte}
        if titre_groupe:
            ingredient["title"] = titre_groupe
            titre_groupe = None
        ingredients.append(ingredient)

    mots_cles = [etiquette] if etiquette else []
    if a["congelable"]:
        mots_cles.append("Congelable")
    mots_cles += [allergenes.nom_tag(x) for x in a["allergenes"]]

    recette = {
        "@context": "https://schema.org",
        "@type": "Recipe",
        "name": nom or a["titre"],
        "description": a["conseil"] or "",
        "recipeIngredient": ingredients,
        "recipeInstructions": _instructions_schemaorg(a["etapes"]),
        "keywords": ",".join(mots_cles),
        "notes": a["notes"],
    }
    if a["portions"]:
        recette["recipeYield"] = f"{a['portions']['nombre']} {a['portions']['unite']}"
    if source.get("url"):
        recette["url"] = source["url"]
    return recette


class SourceExportJson(Source):
    nom = "Export JSON"
    menu_modifiable = True
    corrections = True
    recherche = True

    def __init__(self, dossier_source: Path, nom_json: str, sous_dossier_images: str,
                 dossier_data: Path):
        self.chemin_json = Path(dossier_source) / nom_json
        self.dossier_images = Path(dossier_source) / sous_dossier_images
        self.dossier_cache = Path(dossier_data) / "cache" / "images"
        self.etat = Etat(Path(dossier_data))
        self._verrou = threading.RLock()
        self._signature = None          # (mtime, taille) du JSON chargé
        self._sources: dict[str, dict] = {}
        self._index: dict[str, str] = {}   # id → texte de recherche sans accents
        self._converties: dict[str, dict] = {}
        self.mtime_source = 0

    # --- Chargement ---------------------------------------------------------------

    def _verifier(self) -> None:
        """Recharge la base si le fichier a changé depuis la dernière lecture."""
        st = self.chemin_json.stat()
        signature = (st.st_mtime_ns, st.st_size)
        if signature == self._signature:
            return
        with open(self.chemin_json, "rb") as f:   # lecture seule
            donnees = json.load(f)
        sources = {str(r["id"]): r for r in donnees}
        self._sources = sources
        self._index = {
            rid: sans_accents(
                html_vers_texte(r.get("title")) + " " + html_vers_texte(r.get("ingredientsHtml")))
            for rid, r in sources.items()
        }
        self._converties = {}
        self._signature = signature
        self.mtime_source = int(st.st_mtime)

    def _image_source(self, rid: str) -> Path | None:
        nom = nom_fichier_image(self._sources[rid])
        if not nom:
            return None
        chemin = self.dossier_images / nom
        return chemin if chemin.is_file() else None

    # --- Recettes -------------------------------------------------------------------

    def recette(self, rid: str) -> dict | None:
        with self._verrou:
            self._verifier()
            if rid not in self._sources:
                return None
            if rid not in self._converties:
                self._converties[rid] = convertir_export(
                    self._sources[rid],
                    image_presente=self._image_source(rid) is not None,
                    correction=self.etat.correction(rid),
                )
            return self._converties[rid]

    def sources_brutes(self) -> list[dict]:
        """Toutes les recettes telles que dans l'export (pour l'export Mealie)."""
        with self._verrou:
            self._verifier()
            return list(self._sources.values())

    def image_source(self, rid: str) -> Path | None:
        """Photo d'origine (non réduite)."""
        with self._verrou:
            self._verifier()
            return self._image_source(rid) if rid in self._sources else None

    def chemin_image(self, rid: str) -> Path | None:
        source = self.image_source(rid)
        if source is None:
            return None
        return image_en_cache(source, self.dossier_cache, rid)

    def toutes(self) -> list[dict]:
        """Toutes les recettes converties (pour le rapport et les tests)."""
        with self._verrou:
            self._verifier()
            ids = list(self._sources)
        return [self.recette(rid) for rid in ids]

    # --- Menu, corrections, recherche --------------------------------------------

    def menu(self) -> dict:
        with self._verrou:
            self._verifier()
            ids = [rid for rid in self.etat.ids_menu() if rid in self._sources]
            maj = max(self.etat.maj, self.mtime_source)
        recettes = []
        for rid in ids:
            r = self.recette(rid)
            recettes.append({"id": rid, "titre": r["titre"], "rev": r["rev"], "image": r["image"]})
        return {"v": 1, "maj": maj, "recettes": recettes}

    def ajouter(self, rid: str) -> None:
        self.etat.ajouter(rid)

    def retirer(self, rid: str) -> None:
        self.etat.retirer(rid)

    def definir_portions(self, rid: str, portions: dict | None) -> None:
        self.etat.definir_portions(rid, portions)
        with self._verrou:
            self._converties.pop(rid, None)

    def rechercher(self, requete: str, limite: int = 50) -> dict:
        """Recherche sans accents dans les titres et les ingrédients.

        Tous les mots doivent être présents ; les recettes dont le titre
        contient les mots passent en premier.
        """
        mots = sans_accents(requete).split()
        with self._verrou:
            self._verifier()
            dans_menu = set(self.etat.ids_menu())
            candidats = []
            for rid, texte in self._index.items():
                if all(m in texte for m in mots):
                    titre_sa = sans_accents(html_vers_texte(self._sources[rid].get("title")))
                    score = sum(m in titre_sa for m in mots)
                    candidats.append((-score, titre_sa, rid))
        candidats.sort()
        resultats = []
        for _, _, rid in candidats[:limite]:
            r = self.recette(rid)
            resultats.append({
                "id": rid,
                "titre": r["titre"],
                "dans_menu": rid in dans_menu,
                "portions": r["portions"],
                "image": r["image"],
                "nb_etapes": len(r["etapes"]),
                "nb_minuteurs": len(r["minuteurs"]),
            })
        return {"total": len(candidats), "resultats": resultats}
