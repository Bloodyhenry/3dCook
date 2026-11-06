"""Point d'entrée : python -m recettes3ds

La source est choisie par les variables d'environnement.

Source Mealie (si MEALIE_URL est défini) :
  MEALIE_URL            adresse de l'API, ex. http://mealie:9000
  MEALIE_JETON          jeton d'API (Mealie → Profil → Jetons d'API)
  MEALIE_LIEN           adresse affichée sur la page web, ex. http://nas:9925 (facultatif)
  MENU_JOURS            nombre de jours de planning dans le menu   défaut 7

Source export JSON (sinon) :
  RECETTES_SOURCE       dossier de la base (lecture seule)         défaut /source
  RECETTES_JSON         nom du fichier JSON                        défaut recettes.json
  RECETTES_IMAGES       sous-dossier des photos                    défaut images

Commun :
  RECETTES_DATA         dossier de la passerelle (menu, cache)     défaut /data
  RECETTES_HOTE         adresse d'écoute                           défaut 0.0.0.0
  RECETTES_PORT         port d'écoute                              défaut 8383
"""

import os
import sys
from pathlib import Path

from .serveur import adresse_locale, creer_serveur
from .sources import Source


def source_depuis_env() -> Source:
    data = Path(os.environ.get("RECETTES_DATA", "/data"))
    if os.environ.get("MEALIE_URL"):
        from .sources.mealie import SourceMealie

        jeton = os.environ.get("MEALIE_JETON", "")
        if not jeton:
            # On démarre quand même (pas de redémarrages en boucle dans Docker) :
            # chaque requête répondra 502 « jeton refusé » tant qu'il manque.
            print("ATTENTION : MEALIE_JETON est vide. Créer un jeton dans Mealie "
                  "(Profil → Jetons d'API), le mettre dans .env et relancer.", flush=True)
        source = SourceMealie(os.environ["MEALIE_URL"], jeton, data,
                              jours=int(os.environ.get("MENU_JOURS", "7")),
                              lien=os.environ.get("MEALIE_LIEN") or None)
        print(f"Source : Mealie ({os.environ['MEALIE_URL']})", flush=True)
        return source

    from .sources.export_json import NOM_JSON, SourceExportJson

    source = SourceExportJson(Path(os.environ.get("RECETTES_SOURCE", "/source")),
                              os.environ.get("RECETTES_JSON", NOM_JSON),
                              os.environ.get("RECETTES_IMAGES", "images"), data)
    if not source.chemin_json.is_file():
        sys.exit(f"Base introuvable : {source.chemin_json}")
    print(f"Source : export JSON, {len(source.toutes())} recettes", flush=True)
    return source


def main() -> None:
    hote = os.environ.get("RECETTES_HOTE", "0.0.0.0")
    port = int(os.environ.get("RECETTES_PORT", "8383"))
    serveur = creer_serveur(source_depuis_env(), hote, port)
    print(f"Écoute sur http://{hote}:{port}/", flush=True)
    ip = adresse_locale()
    if ip:
        print(f"Adresse à saisir sur la 3DS : http://{ip}:{port}", flush=True)
    try:
        serveur.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        serveur.server_close()


if __name__ == "__main__":
    main()
