"""Conversion du HTML de la base en lignes de texte propres.

Un export de site web contient du HTML copié-collé (Word, Safari…) : balises
<div>, <p>, <span> imbriquées, espaces insécables, styles en ligne.
On en tire une liste de lignes, chacune avec un drapeau « allergène »
(le site écrit les allergènes en rouge).
"""

import html
import re
import unicodedata

# Marqueur interne posé à la place des balises rouges avant de retirer le HTML.
_MARQUE_ROUGE = "\x01"

# Balises <font>/<span> qui mettent le texte en rouge (toutes les variantes vues
# dans la base : #FF0000, #ff3300, color:red, rgb(255, 0, 0)…).
_RE_ROUGE = re.compile(
    r"<(?:font|span)\b[^>]*(?:#ff0000|#ff3300|#f00\b|\bred\b|rgb\(\s*255\s*,\s*0\s*,\s*0\s*\))[^>]*>",
    re.IGNORECASE,
)
_RE_BLOC = re.compile(r"<br\s*/?>|</?(?:div|p|li|ul|ol|h\d|tr|table)\b[^>]*>", re.IGNORECASE)
_RE_STYLE = re.compile(r"<(style|script)\b[^>]*>.*?</\1>", re.IGNORECASE | re.DOTALL)
_RE_COMMENTAIRE = re.compile(r"<!--.*?-->", re.DOTALL)
_RE_BALISE = re.compile(r"<[^>]+>")

# Remplacements typographiques : on garde des caractères que la police de la
# 3DS affiche à coup sûr et qui restent simples à manipuler en C.
_REMPLACEMENTS = {
    " ": " ",   # espace insécable
    " ": " ",   # espace fine insécable
    " ": " ",   # espace fine
    "⁠": "",    # gluon invisible
    "​": "",    # espace de largeur nulle
    "﻿": "",
    "’": "'",   # apostrophe typographique
    "‘": "'",
    "“": '"',
    "”": '"',
    "…": "...",
    "–": "-",   # tiret demi-cadratin
    "—": "-",
    "•": "-",   # puce
    "·": "-",
}


def normaliser(texte: str) -> str:
    """Nettoie un texte brut : typographie, espaces multiples, NFC."""
    texte = unicodedata.normalize("NFC", texte)
    for avant, apres in _REMPLACEMENTS.items():
        texte = texte.replace(avant, apres)
    return re.sub(r"\s+", " ", texte).strip()


def html_vers_lignes(fragment: str | None) -> list[tuple[str, bool]]:
    """Découpe un fragment HTML en lignes (texte, contient_allergene).

    Les retours à la ligne bruts du HTML sont des espaces ; seules les balises
    de bloc (<div>, <p>, <br>…) créent de nouvelles lignes.
    """
    if not fragment:
        return []
    h = _RE_STYLE.sub(" ", fragment)
    h = _RE_COMMENTAIRE.sub(" ", h)
    h = re.sub(r"\s+", " ", h)
    h = _RE_ROUGE.sub(_MARQUE_ROUGE, h)
    h = _RE_BLOC.sub("\n", h)
    h = _RE_BALISE.sub("", h)
    h = html.unescape(h)

    lignes = []
    for brute in h.split("\n"):
        allergene = _MARQUE_ROUGE in brute
        propre = normaliser(brute.replace(_MARQUE_ROUGE, ""))
        if propre:
            lignes.append((propre, allergene))
    return lignes


_RE_SEGMENT_ROUGE = re.compile(_RE_ROUGE.pattern + r"([^<]*)", re.IGNORECASE)


def segments_rouges(fragment: str | None) -> list[str]:
    """Textes écrits en rouge (« poisson », « farine de blé »…), dans l'ordre."""
    if not fragment:
        return []
    segments = (normaliser(html.unescape(m.group(1))) for m in _RE_SEGMENT_ROUGE.finditer(fragment))
    return [s for s in segments if s.strip(" -,.;:()'")]


def html_vers_texte(fragment: str | None) -> str:
    """Version « paragraphes » d'un fragment HTML (lignes jointes par \\n)."""
    return "\n".join(t for t, _ in html_vers_lignes(fragment))


def sans_accents(texte: str) -> str:
    """Minuscules sans accents, pour la recherche (« Œufs » → « oeufs »)."""
    texte = texte.lower().replace("œ", "oe").replace("æ", "ae")
    decompose = unicodedata.normalize("NFD", texte)
    return "".join(c for c in decompose if unicodedata.category(c) != "Mn")
