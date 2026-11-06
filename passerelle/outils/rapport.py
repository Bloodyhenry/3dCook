"""Rapport d'analyse de toute la base : à lancer après chaque modification
de l'analyseur pour repérer les cas douteux.

    python outils/rapport.py ../recettes [--json recettes.json] [--detail ID]
"""

import argparse
import json
import sys
import tempfile
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.construire import calculer_rev  # noqa: E402
from recettes3ds.sources.export_json import NOM_JSON, SourceExportJson  # noqa: E402


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("source")
    p.add_argument("--json", default=NOM_JSON)
    p.add_argument("--detail", help="affiche la recette convertie de cet id")
    args = p.parse_args()

    with tempfile.TemporaryDirectory() as data:
        biblio = SourceExportJson(Path(args.source), args.json, "images", Path(data))
        if args.detail:
            print(json.dumps(biblio.recette(args.detail), ensure_ascii=False, indent=1))
            return
        recettes = biblio.toutes()

    n = len(recettes)
    items = [i for r in recettes for g in r["groupes"] for i in g["items"]]
    sans_q = [i["nom"] for i in items if i["quantite"] is None]
    unites = Counter(i["unite"] for i in items if i["quantite"] is not None)
    tailles = sorted(len(json.dumps(r, ensure_ascii=False).encode()) for r in recettes)
    etapes = [e["texte"] for r in recettes for e in r["etapes"]]
    minuteurs = [m for r in recettes for m in r["minuteurs"]]

    print(f"Recettes : {n}")
    print(f"Portions détectées : {sum(r['portions'] is not None for r in recettes)}")
    print(Counter(r["portions"]["unite"] for r in recettes if r["portions"]).most_common(25))
    print(f"Ingrédients : {len(items)} dont {len(sans_q)} sans quantité")
    print(f"Unités : {unites.most_common()}")
    print(f"Étapes : {len(etapes)} (max/recette {max(len(r['etapes']) for r in recettes)}, "
          f"recettes sans étape : {sum(not r['etapes'] for r in recettes)})")
    print(f"Étapes > 200 car. : {sum(len(e) > 200 for e in etapes)}")
    print(f"Minuteurs : {len(minuteurs)} dans {sum(bool(r['minuteurs']) for r in recettes)} recettes")
    print(Counter(m["libelle"].rsplit(" ", 2)[0] for m in minuteurs).most_common(20))
    print(f"Notes : {sum(len(r['notes']) for r in recettes)}")
    print(f"Taille JSON : médiane {tailles[n // 2]} o, max {tailles[-1]} o")
    print(f"Rev cohérentes : {all(calculer_rev(r) == r['rev'] for r in recettes)}")


if __name__ == "__main__":
    main()
