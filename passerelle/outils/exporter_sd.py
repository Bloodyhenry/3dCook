"""Prépare des recettes à copier sur la carte SD de la 3DS (étape 2 de l'appli).

Depuis un export JSON local :
    python outils/exporter_sd.py ../recettes sd --ids 416 388
    python outils/exporter_sd.py ../recettes sd --tout

Depuis une passerelle qui tourne (Mealie ou export JSON), recettes du menu :
    python outils/exporter_sd.py --url http://192.168.1.10:8383 sd

Résultat : sd/3ds/recettes/cache/<id>.json (+ <id>.jpg quand il y a une
photo). Copier le dossier « 3ds » de sd/ à la racine de la carte SD.
"""

import argparse
import json
import shutil
import sys
import tempfile
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.sources.export_json import NOM_JSON, SourceExportJson  # noqa: E402

SOUS_DOSSIER = Path("3ds") / "recettes" / "cache"


def ecrire_recette(dossier: Path, recette: dict) -> Path:
    """JSON compact en UTF-8, comme le sert la passerelle."""
    chemin = dossier / f"{recette['id']}.json"
    chemin.write_text(json.dumps(recette, ensure_ascii=False, separators=(",", ":")),
                      encoding="utf-8")
    return chemin


def depuis_export(source: Path, sortie: Path, ids: list[str] | None,
                  nom_json: str = NOM_JSON) -> int:
    dossier = sortie / SOUS_DOSSIER
    dossier.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as data:
        base = SourceExportJson(source, nom_json, "images", Path(data))
        if ids is None:
            ids = [str(r["id"]) for r in base.sources_brutes()]
        n = 0
        for rid in ids:
            recette = base.recette(rid)
            if recette is None:
                print(f"Recette inconnue : {rid}", file=sys.stderr)
                continue
            ecrire_recette(dossier, recette)
            image = base.chemin_image(rid)
            if image:
                shutil.copyfile(image, dossier / f"{rid}.jpg")
            n += 1
    return n


def depuis_passerelle(url: str, sortie: Path) -> int:
    dossier = sortie / SOUS_DOSSIER
    dossier.mkdir(parents=True, exist_ok=True)
    url = url.rstrip("/")

    def lire(chemin: str) -> bytes:
        with urllib.request.urlopen(url + chemin, timeout=15) as rep:
            return rep.read()

    menu = json.loads(lire("/menu"))
    for entree in menu["recettes"]:
        recette = json.loads(lire(f"/recipes/{entree['id']}"))
        ecrire_recette(dossier, recette)
        if recette.get("image"):
            (dossier / f"{entree['id']}.jpg").write_bytes(lire(f"/recipes/{entree['id']}/image"))
    return len(menu["recettes"])


def main() -> None:
    p = argparse.ArgumentParser(description="Prépare des recettes pour la carte SD de la 3DS.")
    p.add_argument("source", type=Path, nargs="?", help="dossier de l'export JSON")
    p.add_argument("--json", default=NOM_JSON, help="nom du fichier JSON dans le dossier")
    p.add_argument("sortie", type=Path, help="dossier de sortie (ex. sd)")
    p.add_argument("--ids", nargs="+", help="identifiants des recettes")
    p.add_argument("--tout", action="store_true", help="toutes les recettes de l'export")
    p.add_argument("--url", help="adresse d'une passerelle : prend les recettes de son menu")
    args = p.parse_args()

    if args.url:
        n = depuis_passerelle(args.url, args.sortie)
    elif args.source and (args.ids or args.tout):
        n = depuis_export(args.source, args.sortie, None if args.tout else args.ids, args.json)
    else:
        p.error("indiquer --url, ou un dossier source avec --ids ou --tout")
    print(f"{n} recette(s) dans {args.sortie / SOUS_DOSSIER}")
    print(f"Copier le dossier « 3ds » de {args.sortie} à la racine de la carte SD.")


if __name__ == "__main__":
    main()
