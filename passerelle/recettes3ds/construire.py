"""Assemblage du JSON 3DS, commun à toutes les sources de recettes.

Chaque source (export JSON, Mealie…) fournit des données déjà
découpées : titre, groupes d'ingrédients, étapes. Ce module ajoute les
minuteurs et l'empreinte « rev ». Le format est décrit dans docs/schema.md.
"""

import hashlib
import json

from . import VERSION_SCHEMA
from .minuteurs import libelle_minuteur, minuteurs_etape


def calculer_rev(recette: dict) -> str:
    """Empreinte courte du contenu converti : change dès que la recette change."""
    sans_rev = {k: v for k, v in recette.items() if k != "rev"}
    brut = json.dumps(sans_rev, ensure_ascii=False, sort_keys=True).encode("utf-8")
    return hashlib.sha1(brut).hexdigest()[:8]


def construire(*, rid: str, titre: str, portions: dict | None, congelable: bool,
               image: bool, groupes: list[dict], etapes: list[dict],
               notes: list[str], conseil: str | None) -> dict:
    """Recette au format 3DS.

    `etapes` : [{"texte": …, "section": … (facultatif), "secondes": … (facultatif)}].
    Une étape qui porte déjà sa durée (« secondes », saisie dans Mealie) donne
    directement son minuteur ; sinon on cherche les durées dans le texte.
    """
    minuteurs = []
    etapes_sortie = []
    for i, e in enumerate(etapes):
        if e.get("secondes"):
            minuteurs.append({
                "etape": i,
                "secondes": int(e["secondes"]),
                "secondes_max": None,
                "libelle": libelle_minuteur(e["texte"], i, int(e["secondes"]), None),
            })
        else:
            minuteurs.extend(minuteurs_etape(e["texte"], i))
        sortie = {"texte": e["texte"]}
        if e.get("section"):
            sortie["section"] = e["section"]
        etapes_sortie.append(sortie)

    recette = {
        "v": VERSION_SCHEMA,
        "id": rid,
        "titre": titre,
        "rev": "",
        "portions": portions,
        "congelable": congelable,
        "image": image,
        "groupes": groupes or [{"titre": None, "items": []}],
        "etapes": etapes_sortie,
        "minuteurs": minuteurs,
        "notes": notes,
        "conseil": conseil or None,
    }
    recette["rev"] = calculer_rev(recette)
    return recette
