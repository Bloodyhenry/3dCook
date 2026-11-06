"""Le NAS fait-il tourner la même passerelle que ce Mac ?

    python outils/verifier_nas.py http://192.168.1.37:8383

Compare l'empreinte du code local (recettes3ds/) à celle que la passerelle du
NAS donne dans /api/info. Code de sortie : 0 à jour, 1 ancien code, 2 injoignable.
"""

import json
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds import empreinte_code  # noqa: E402


def main() -> int:
    if len(sys.argv) != 2:
        print("usage : verifier_nas.py http://adresse-du-nas:8383", file=sys.stderr)
        return 2
    url = sys.argv[1].rstrip("/")
    locale = empreinte_code()
    try:
        with urllib.request.urlopen(f"{url}/api/info", timeout=8) as rep:
            info = json.loads(rep.read().decode("utf-8"))
    except OSError as e:
        print(f"Passerelle injoignable ({url}) : {e}")
        print("Le projet est peut-être en cours de démarrage : réessayer dans une minute.")
        return 2

    distante = info.get("code")
    menu = info.get("menu", "page web")
    if distante == locale:
        print(f"À jour : le NAS fait tourner la même passerelle que ce Mac ({locale}).")
        print(f"Source : {info.get('source')}, menu : {menu}.")
        return 0
    print(f"Ancien code sur le NAS ({distante or 'version sans empreinte'}, ce Mac : {locale}).")
    print("Si « make deployer-nas » a été lancé : dans Container Manager, projet recettes3ds,")
    print("Arrêter puis Construire. Si ça ne suffit pas, supprimer d'abord l'image recettes3ds")
    print("(onglet Image), ou ajouter « pull_policy: build » au compose.yaml du NAS.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
