"""Export d'une base de recettes (export JSON) vers une archive importable dans Mealie.

    python outils/exporter_mealie.py ../recettes recettes-mealie.zip [--etiquette Import]

L'archive suit le format Nextcloud Cookbook : un dossier par recette avec
recipe.json (schema.org Recipe) et full.jpg. Dans Mealie :
Paramètres → Migrations → « Nextcloud Cookbook » → envoyer l'archive.

La base d'origine n'est que lue ; l'archive doit être écrite ailleurs.
"""

import argparse
import json
import re
import sys
import unicodedata
import zipfile
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.sources.export_json import NOM_JSON, analyser, nom_fichier_image, vers_schemaorg  # noqa: E402


def slug(texte: str) -> str:
    """« Acras de poisson » → « acras-de-poisson » (nom de dossier sûr)."""
    t = texte.lower().replace("œ", "oe").replace("æ", "ae")
    t = unicodedata.normalize("NFKD", t).encode("ascii", "ignore").decode()
    return re.sub(r"[^a-z0-9]+", "-", t).strip("-")[:60] or "recette"


def noms_uniques(titres: list[str]) -> list[str]:
    """Mealie fusionne les recettes de même nom : on numérote les doublons."""
    vus: Counter = Counter()
    sortie = []
    for t in titres:
        vus[t.lower()] += 1
        sortie.append(t if vus[t.lower()] == 1 else f"{t} ({vus[t.lower()]})")
    return sortie


def exporter(dossier_source: Path, nom_json: str, sortie: Path,
             etiquette: str | None = None) -> dict:
    """Écrit l'archive et renvoie un petit bilan."""
    dossier_source = dossier_source.resolve()
    if dossier_source in sortie.resolve().parents:
        raise SystemExit("L'archive ne doit pas être écrite dans le dossier de la base.")

    with open(dossier_source / nom_json, "rb") as f:   # lecture seule
        recettes = json.load(f)
    titres = noms_uniques([analyser(r)["titre"] for r in recettes])

    bilan = {"recettes": 0, "photos": 0, "allergenes": Counter(),
             "rouges_non_reconnus": Counter(), "sans_portions": 0}
    with zipfile.ZipFile(sortie, "w", zipfile.ZIP_DEFLATED) as zf:
        for r, titre in zip(recettes, titres):
            dossier = f"recettes/{r['id']}-{slug(titre)}"
            schema = vers_schemaorg(r, nom=titre, etiquette=etiquette)
            zf.writestr(f"{dossier}/recipe.json",
                        json.dumps(schema, ensure_ascii=False, indent=1))
            bilan["recettes"] += 1

            nom_image = nom_fichier_image(r)
            image = dossier_source / "images" / nom_image if nom_image else None
            if image and image.is_file():
                extension = image.suffix.lower().lstrip(".") or "jpg"
                zf.write(image, f"{dossier}/full.{extension}", compress_type=zipfile.ZIP_STORED)
                bilan["photos"] += 1

            a = analyser(r)
            bilan["allergenes"].update(a["allergenes"])
            bilan["rouges_non_reconnus"].update(s.lower() for s in a["rouges_non_reconnus"])
            bilan["sans_portions"] += a["portions"] is None
    return bilan


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    p.add_argument("source", type=Path, help="dossier de la base (JSON + images/)")
    p.add_argument("sortie", type=Path, help="archive .zip à créer")
    p.add_argument("--json", default=NOM_JSON, help="nom du fichier JSON dans le dossier")
    p.add_argument("--etiquette", help="étiquette Mealie ajoutée à chaque recette")
    args = p.parse_args()

    bilan = exporter(args.source, args.json, args.sortie, args.etiquette or None)
    print(f"{bilan['recettes']} recettes, {bilan['photos']} photos → {args.sortie}")
    print(f"Sans portions : {bilan['sans_portions']} (à compléter dans Mealie)")
    print("Allergènes :", ", ".join(f"{k} {v}" for k, v in bilan["allergenes"].most_common()))
    if bilan["rouges_non_reconnus"]:
        print("Mots en rouge non classés :",
              ", ".join(f"{k} ({v})" for k, v in bilan["rouges_non_reconnus"].most_common()))


if __name__ == "__main__":
    main()
