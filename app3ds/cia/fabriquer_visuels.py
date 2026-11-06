"""Fabrique l'icône, la bannière et le son du menu HOME pour le paquet CIA.

    ../passerelle/.venv/bin/python cia/fabriquer_visuels.py

Produit (dans app3ds/) :
  icon.png            48 × 48  icône (Homebrew Launcher et menu HOME)
  cia/banniere.png    256 × 128 bannière affichée en haut quand on choisit l'appli
  cia/banniere.wav    son court joué avec la bannière (≤ 3 s)

Tout est dessiné par le script (aucune image externe) : une marmite qui fume,
aux couleurs de l'appli. Il faut Pillow (déjà présent dans l'environnement de
la passerelle).
"""

import math
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ICI = Path(__file__).resolve().parent
RACINE = ICI.parent

FOND = (28, 26, 23)
ACCENT = (224, 119, 76)
CREME = (242, 237, 228)
DOUX = (165, 155, 143)

POLICE = "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf"


def marmite(d: ImageDraw.ImageDraw, x: float, y: float, t: float, couleur, fond) -> None:
    """Marmite de taille `t` (largeur), coin haut gauche (x, y), avec sa vapeur."""
    e = t / 48  # unité : 1/48 de la largeur
    # Vapeur : trois volutes au-dessus.
    for i, dx in enumerate((14, 24, 34)):
        pts = []
        for k in range(13):
            yy = y + (4 + k * 1.0) * e
            xx = x + (dx + 2.2 * math.sin(k / 12 * 2 * math.pi + i)) * e
            pts.append((xx, yy))
        d.line(pts, fill=couleur, width=max(1, round(2.2 * e)), joint="curve")
    # Couvercle et bouton.
    d.rounded_rectangle([x + 18 * e, y + 18 * e, x + 30 * e, y + 21 * e], radius=1.5 * e, fill=couleur)
    d.rounded_rectangle([x + 6 * e, y + 21 * e, x + 42 * e, y + 25 * e], radius=2 * e, fill=couleur)
    # Corps et anses.
    d.rounded_rectangle([x + 8 * e, y + 26 * e, x + 40 * e, y + 44 * e], radius=5 * e, fill=couleur)
    d.rounded_rectangle([x + 1 * e, y + 29 * e, x + 9 * e, y + 33 * e], radius=2 * e, fill=couleur)
    d.rounded_rectangle([x + 39 * e, y + 29 * e, x + 47 * e, y + 33 * e], radius=2 * e, fill=couleur)
    # Reflet.
    d.rounded_rectangle([x + 12 * e, y + 30 * e, x + 15 * e, y + 39 * e], radius=1.5 * e, fill=fond)


def icone() -> None:
    n = 4  # suréchantillonnage pour des bords lisses
    img = Image.new("RGB", (48 * n, 48 * n), FOND)
    d = ImageDraw.Draw(img)
    # Carré plein : le menu HOME dessine lui-même le cadre de l'icône.
    d.rectangle([0, 0, 48 * n - 1, 48 * n - 1], fill=ACCENT)
    marmite(d, 3 * n, 2 * n, 42 * n, CREME, ACCENT)
    img.resize((48, 48), Image.Resampling.LANCZOS).save(RACINE / "icon.png")


def banniere() -> None:
    n = 2
    img = Image.new("RGB", (256 * n, 128 * n), FOND)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([8 * n, 16 * n, 104 * n, 112 * n], radius=18 * n, fill=ACCENT)
    marmite(d, 14 * n, 20 * n, 84 * n, CREME, ACCENT)
    # Chaque texte prend la plus grande taille qui tient dans la largeur restante.
    largeur = (256 - 116 - 6) * n

    def police(texte: str, taille: int) -> ImageFont.FreeTypeFont:
        while taille > 6 and d.textlength(texte, font=ImageFont.truetype(POLICE, taille * n)) > largeur:
            taille -= 1
        return ImageFont.truetype(POLICE, taille * n)

    d.text((114 * n, 34 * n), "Recettes", font=police("Recettes", 32), fill=CREME)
    d.text((116 * n, 76 * n), "sans les mains", font=police("sans les mains", 15), fill=ACCENT)
    d.text((116 * n, 96 * n), "souffle, main, minuteurs", font=police("souffle, main, minuteurs", 10),
           fill=DOUX)
    img.resize((256, 128), Image.Resampling.LANCZOS).save(ICI / "banniere.png")


def son() -> None:
    """Deux notes douces (mi puis la), 0,8 s, mono 16 bits à 22 050 Hz."""
    frequence = 22050
    echantillons = []
    for debut, note in ((0.0, 659.25), (0.18, 880.0)):
        for i in range(int(0.62 * frequence)):
            t = i / frequence
            enveloppe = min(1.0, t / 0.01) * math.exp(-t * 6)
            echantillons.append((int(debut * frequence) + i,
                                 0.45 * enveloppe * math.sin(2 * math.pi * note * t)))
    total = [0.0] * int(0.8 * frequence)
    for i, v in echantillons:
        if i < len(total):
            total[i] += v
    with wave.open(str(ICI / "banniere.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(frequence)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32767)) for v in total))


if __name__ == "__main__":
    icone()
    banniere()
    son()
    print("icon.png, cia/banniere.png et cia/banniere.wav créés.")
