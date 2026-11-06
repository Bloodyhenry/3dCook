"""Passerelle entre la base de recettes du NAS et l'appli 3DS.

La base d'origine est lue en lecture seule. Tout ce que la passerelle écrit
(menu de la semaine, corrections, cache d'images) va dans un dossier à part.
"""

import hashlib
from pathlib import Path

VERSION_SCHEMA = 1


def empreinte_code() -> str:
    """Empreinte courte du code de la passerelle (fichiers .py et .html du paquet).

    Exposée dans /api/info : `make verifier-nas` la compare à celle du Mac pour
    savoir si le NAS fait tourner la dernière version.
    """
    racine = Path(__file__).resolve().parent
    h = hashlib.sha1()
    for chemin in sorted(racine.rglob("*")):
        if chemin.suffix in (".py", ".html") and "__pycache__" not in chemin.parts:
            h.update(chemin.relative_to(racine).as_posix().encode())
            h.update(chemin.read_bytes())
    return h.hexdigest()[:10]
