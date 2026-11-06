"""Analyse des lignes d'ingrédients et détection des portions.

Une ligne « - 220 g de poisson blanc » devient :
  {"quantite": 220, "quantite_max": None, "unite": "g",
   "nom": "de poisson blanc", "allergene": True}
Le « de » reste dans le nom pour que l'affichage soit du français naturel.
Une ligne qu'on ne sait pas lire garde tout son texte dans « nom » et
quantite = None : elle ne sera jamais mise à l'échelle.
"""

import re

from .texte import sans_accents

_MOTS_NOMBRES = {
    "un": 1, "une": 1, "deux": 2, "trois": 3, "quatre": 4, "cinq": 5,
    "six": 6, "sept": 7, "huit": 8, "neuf": 9, "dix": 10, "douze": 12,
}
_FRACTIONS = {"½": 0.5, "¼": 0.25, "¾": 0.75, "⅓": 1 / 3, "⅔": 2 / 3}

# 1 ½ | 1½ | 1 1/2 | 1/2 | 1,5 | 1.5 | 12 | ½ | deux
_NUM = (
    r"\d+\s*[½¼¾⅓⅔]|\d+\s+\d+/\d+|\d+/\d+|\d+(?:[.,]\d+)?|[½¼¾⅓⅔]"
    r"|(?:douze|dix|neuf|huit|sept|six|cinq|quatre|trois|deux|une|un)\b"
)

# Unités reconnues : (motif, forme écrite dans le JSON).
# Les formes comptables sont au singulier ; l'appli ajoute le pluriel.
_UNITES = [
    (r"kg|kilos?|kilogrammes?", "kg"),
    (r"mg", "mg"),
    (r"g|gr|grs|grammes?", "g"),
    (r"ml|millilitres?", "ml"),
    (r"cl|centilitres?", "cl"),
    (r"dl|décilitres?", "dl"),
    (r"l|litres?", "l"),
    (r"(?:(?:grosses?|petites?|bonnes?)\s+)?(?:cuill[eè]res?|cuil\.?|c\.?)\s*(?:à|a)\s*(?:soupe|s\.?)|cas|càs|cs|c\.s\.", "c. à soupe"),
    (r"(?:(?:grosses?|petites?|bonnes?)\s+)?(?:cuill[eè]res?|cuil\.?|c\.?)\s*(?:à|a)\s*(?:café|cafe|c\.?)|cac|càc|cc|c\.c\.", "c. à café"),
    (r"pinc[ée]es?", "pincée"),
    (r"verres?", "verre"),
    (r"tasses?", "tasse"),
    (r"bols?", "bol"),
    (r"petits?\s+pots?", "petit pot"),
    (r"pots?", "pot"),
    (r"sachets?", "sachet"),
    (r"tranches?", "tranche"),
    (r"gousses?", "gousse"),
    (r"bottes?", "botte"),
    (r"brins?", "brin"),
    (r"branches?", "branche"),
    (r"poign[ée]es?", "poignée"),
    (r"morceaux|morceau", "morceau"),
    (r"bo[iî]tes?", "boîte"),
    (r"briques?", "brique"),
    (r"feuilles?", "feuille"),
    (r"carr[ée]s?", "carré"),
    (r"rondelles?", "rondelle"),
    (r"tiges?", "tige"),
    (r"tablettes?", "tablette"),
    (r"noix(?=\s+de\s+beurre)", "noix"),
    (r"filets?", "filet"),
    (r"bouquets?", "bouquet"),
    (r"zestes?", "zeste"),
    (r"doses?", "dose"),
    (r"mesures?", "mesure"),
    (r"barquettes?", "barquette"),
    (r"bocaux|bocal", "bocal"),
    (r"boules?", "boule"),
]
_RE_UNITE = re.compile(
    # Chaque unité doit être un mot entier : « 2 gros œufs » n'a pas l'unité « g ».
    "|".join(rf"(?P<u{i}>(?:{motif})(?![^\W\d_]))" for i, (motif, _) in enumerate(_UNITES)),
    re.IGNORECASE,
)
_RE_QUANTITE = re.compile(
    rf"^(?:environ\s+|~\s*)?(?P<q>{_NUM})(?:\s*(?:-|à|a|ou)\s*(?P<q2>{_NUM}))?(?![\d%°/])",
    re.IGNORECASE,
)
_RE_PUCE = re.compile(r"^[-*+>•·]+\s*")


def lire_nombre(texte: str) -> float | None:
    """« 1 ½ », « 1/2 », « 1,5 », « deux » → nombre ; None si illisible."""
    t = texte.strip().lower().replace(",", ".")
    if t in _MOTS_NOMBRES:
        return float(_MOTS_NOMBRES[t])
    if t in _FRACTIONS:
        return _FRACTIONS[t]
    m = re.fullmatch(r"(\d+)\s*([½¼¾⅓⅔])", t)
    if m:
        return int(m[1]) + _FRACTIONS[m[2]]
    m = re.fullmatch(r"(?:(\d+)\s+)?(\d+)/(\d+)", t)
    if m:
        if int(m[3]) == 0:
            return None
        return int(m[1] or 0) + int(m[2]) / int(m[3])
    try:
        return float(t)
    except ValueError:
        return None


def _arrondi_json(v: float | None) -> float | int | None:
    """Nombre compact pour le JSON : 220.0 → 220, 0.333333 → 0.333."""
    if v is None:
        return None
    v = round(v, 3)
    return int(v) if v == int(v) else v


def est_titre_groupe(ligne: str) -> bool:
    """« Pour la pâte : », « Version vegan : » → titre de groupe."""
    t = _RE_PUCE.sub("", ligne).strip()
    return t.endswith(":") and len(t) <= 70 and not re.match(r"^\d", t)


def analyser_ligne(ligne: str, allergene: bool = False) -> dict:
    """Analyse une ligne d'ingrédient (sans titre de groupe)."""
    texte = _RE_PUCE.sub("", ligne).strip()
    item = {"quantite": None, "quantite_max": None, "unite": None,
            "nom": texte, "allergene": allergene}

    m = _RE_QUANTITE.match(texte)
    if not m:
        return item
    q = lire_nombre(m["q"])
    q2 = lire_nombre(m["q2"]) if m["q2"] else None
    if q is None or q <= 0:
        return item

    reste = texte[m.end():].lstrip()
    unite = None
    mu = _RE_UNITE.match(reste)
    if mu:
        indice = int(mu.lastgroup[1:])
        unite = _UNITES[indice][1]
        reste = reste[mu.end():].lstrip(" .")
    nom = reste.strip()
    if not nom:
        return item

    item.update({
        "quantite": _arrondi_json(q),
        "quantite_max": _arrondi_json(q2) if q2 and q2 > q else None,
        "unite": unite,
        "nom": nom,
    })
    return item


def analyser_ingredients(lignes: list[tuple[str, bool]]) -> list[dict]:
    """Regroupe les lignes en groupes {"titre", "items"}.

    Il y a toujours au moins un groupe ; le premier peut avoir titre=None.
    """
    groupes = [{"titre": None, "items": []}]
    for texte, allergene in lignes:
        if est_titre_groupe(texte):
            titre = _RE_PUCE.sub("", texte).rstrip(" :").strip()
            if groupes[-1]["items"] or groupes[-1]["titre"]:
                groupes.append({"titre": titre, "items": []})
            else:
                groupes[-1]["titre"] = titre
            continue
        groupes[-1]["items"].append(analyser_ligne(texte, allergene))
    # Un titre suivi d'aucun ingrédient (fin de liste) ne sert à rien.
    return [g for g in groupes if g["items"]] or [{"titre": None, "items": []}]


# --- Portions ---------------------------------------------------------------

# Mots qui suivent « pour N » sans être une portion (« pour 2 min », « pour 1 l »).
_PAS_UNE_PORTION = {
    "min", "minute", "minutes", "mn", "h", "heure", "heures", "seconde", "secondes",
    "g", "gr", "kg", "ml", "cl", "l", "litre", "litres", "cm", "mois", "an", "ans",
    "jour", "jours", "semaine", "semaines", "fois", "de", "du", "des", "la", "le",
    "les", "un", "une", "a", "et", "ou", "en", "c", "cuillere", "cuilleres",
    "degres", "minutes,", "portion",
}
_RE_PORTIONS = re.compile(
    r"\b(?:pour|environ|soit)\s+(?:environ\s+)?(\d{1,3})(?:\s*(?:-|à|a)\s*\d{1,3})?\s+"
    r"((?:petit(?:e)?s?\s+|grand(?:e)?s?\s+|gros(?:se)?s?\s+|mini[s-]?\s*)?"
    r"[a-zàâäçéèêëîïôöùûüœ'-]+"
    # Complément éventuel : « moules à muffins », « plaque de toasts ».
    r"(?:\s+(?:à|de)\s+[a-zàâäçéèêëîïôöùûüœ-]+)?)",
    re.IGNORECASE,
)


def detecter_portions(*textes: str) -> dict | None:
    """Cherche « pour 6 ramequins », « environ 12 muffins »… dans les textes.

    Les textes sont essayés dans l'ordre donné (ingrédients, puis titre,
    puis préparation) ; on garde la première trouvaille plausible.
    """
    for texte in textes:
        for m in _RE_PORTIONS.finditer(texte or ""):
            # « 340 g de banane (environ 4 bananes) » : précision sur un
            # ingrédient, pas un nombre de portions.
            avant = texte[: m.start()]
            if avant.count("(") > avant.count(")"):
                continue
            nombre = int(m[1])
            unite = m[2].strip().lower().rstrip(":,.")
            mots = sans_accents(unite).split()
            if nombre <= 0 or nombre > 200 or {mots[0], mots[-1]} & _PAS_UNE_PORTION:
                continue
            if m.group(0).lower().startswith("environ") and len(mots[-1]) < 4:
                continue
            return {"nombre": nombre, "unite": unite}
    return None
