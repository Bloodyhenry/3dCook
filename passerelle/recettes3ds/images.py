"""Préparation des photos pour la 3DS.

Une photo par recette, réduite pour tenir dans 400×240 (l'écran du haut),
en JPEG « baseline » (non progressif) qualité 80 : environ 20 à 30 Ko,
simple et rapide à décoder sur l'ARM11.
Les fichiers d'origine ne sont que lus ; le résultat va dans le cache
de la passerelle.
"""

import io
import os
import tempfile
from pathlib import Path
from typing import BinaryIO

from PIL import Image, ImageOps

LARGEUR_MAX = 400
HAUTEUR_MAX = 240
QUALITE = 80


def reduire(source: Path | BinaryIO, destination: Path) -> None:
    """Écrit `destination` (JPEG ≤ 400×240) à partir de `source`.

    `source` est un chemin ou un fichier ouvert (JPEG, PNG, WebP…).
    """
    with Image.open(source) as img:
        img = ImageOps.exif_transpose(img)
        img = img.convert("RGB")
        img.thumbnail((LARGEUR_MAX, HAUTEUR_MAX), Image.Resampling.LANCZOS)
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Écriture atomique : fichier temporaire puis renommage.
        fd, tmp = tempfile.mkstemp(dir=destination.parent, suffix=".tmp")
        try:
            with os.fdopen(fd, "wb") as f:
                img.save(f, "JPEG", quality=QUALITE, optimize=True,
                         progressive=False, subsampling="4:2:0")
            os.replace(tmp, destination)
        except BaseException:
            if os.path.exists(tmp):
                os.unlink(tmp)
            raise


def image_en_cache(source: Path, dossier_cache: Path, cle: str) -> Path:
    """Chemin de la photo réduite, régénérée si la source est plus récente."""
    destination = dossier_cache / f"{cle}.jpg"
    try:
        a_jour = destination.stat().st_mtime >= source.stat().st_mtime
    except FileNotFoundError:
        a_jour = False
    if not a_jour:
        reduire(source, destination)
    return destination


def image_depuis_octets(octets: bytes, destination: Path) -> Path:
    """Réduit une image reçue par le réseau (WebP de Mealie…) vers `destination`."""
    reduire(io.BytesIO(octets), destination)
    return destination
