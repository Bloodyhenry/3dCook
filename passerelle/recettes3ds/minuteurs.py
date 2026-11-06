"""Extraction des minuteurs depuis le texte des étapes.

Exemples reconnus : « 20 min », « 15-20min », « 20 à 30 minutes »,
« entre 30 minutes et 1 nuit » (→ 30 min), « 1h30 », « 1 heure et demie »,
« une demi-heure », « un quart d'heure », « 1/2 heure », « 30 secondes ».

Règles anti faux positifs :
  - au plus une durée par phrase ;
  - une durée entre parenthèses ne sert que s'il n'y en a pas d'autre ;
  - on ignore les phrases « à côté » (« Vous pouvez… », « Attention… ») ;
  - une même durée n'est comptée qu'une fois par étape ;
  - on ignore les durées nulles ou de plus de 12 h (« 48h au frigo »).
Pour une plage, le minuteur démarre sur la valeur basse (secondes) et la
valeur haute est conservée à part (secondes_max).
"""

import re

from .texte import sans_accents

DUREE_MAX = 12 * 3600

_MOTS_NOMBRES = {
    "un": 1, "une": 1, "deux": 2, "trois": 3, "quatre": 4, "cinq": 5,
    "six": 6, "sept": 7, "huit": 8, "neuf": 9, "dix": 10, "quinze": 15,
    "vingt": 20, "trente": 30, "quarante": 40, "quarante-cinq": 45,
}

# Nombre : 20, 1,5, 1.5, 1/2 ou un nombre écrit en lettres.
_NUM = (
    r"\d+/\d+|\d+(?:[.,]\d+)?|quarante-cinq|quarante|trente|vingt|quinze|dix"
    r"|neuf|huit|sept|six|cinq|quatre|trois|deux|une|un"
)
# Unité de temps, jamais suivie d'une lettre (« min » oui, « minces » non).
_UNITE = r"(?:heures?|h|minutes?|min|mn|secondes?|sec|s)(?![^\W\d_])"

_RE_ATOME = re.compile(
    rf"(?<![\w.,/])(?P<v>{_NUM})\s*(?P<u>{_UNITE})"
    rf"(?:\s*(?P<m>\d{{1,2}})(?![\d°%]|[.,]\d)(?:\s*(?:min|mn)(?![^\W\d_]))?)?"
    rf"(?P<demi>\s+et\s+demie?)?",
    re.IGNORECASE,
)
# Expressions sans chiffre.
_RE_SPECIAL = re.compile(
    r"(?<![\w])(?:(?:une|1)\s+)?demi-?\s*heure|(?:un\s+)?quart\s+d'\s*heure",
    re.IGNORECASE,
)
# Début de plage juste avant une durée : « 15- », « 20 à », « entre 20 et »,
# « 15 min - ».
_RE_DEBUT_PLAGE = re.compile(
    rf"(?:entre\s+)?(?<![\w.,/])(?P<v>{_NUM})\s*(?:(?P<u>{_UNITE})(?:\s*(?P<m>\d{{1,2}}))?)?"
    r"\s*(?:-|à|a|et|ou)\s*$",
    re.IGNORECASE,
)
# Phrases qui ne décrivent pas une action à minuter.
# (« Se conserve 3 jours » ne gêne pas : les jours ne sont pas une unité de
# temps reconnue et « 48h » dépasse la durée maximale.)
_RE_APARTE = re.compile(
    r"^(?:vous pouvez|on peut|il est possible|astuce|possibilite|pour info|attention)",
)
_RE_TEMPERATURE = re.compile(r"(\d{2,3})\s*°")


def _nombre(texte: str) -> float:
    t = texte.lower().replace(",", ".")
    if t in _MOTS_NOMBRES:
        return float(_MOTS_NOMBRES[t])
    if "/" in t:
        num, den = t.split("/")
        return float(num) / float(den) if float(den) else 0.0
    return float(t)


def _secondes_unite(unite: str | None) -> int:
    u = (unite or "").lower()
    if u.startswith("h"):
        return 3600
    if u.startswith("m"):
        return 60
    return 1


def _valeur(v: str, u: str | None, m: str | None, demi: bool) -> int:
    """Convertit (nombre, unité, minutes après « h », « et demie ») en secondes."""
    par = _secondes_unite(u)
    total = _nombre(v) * par
    if m and par == 3600:
        total += int(m) * 60
    if demi:
        total += par / 2
    return int(round(total))


def extraire_durees(phrase: str) -> list[dict]:
    """Toutes les durées d'une phrase, dans l'ordre.

    Chaque durée : {"secondes", "secondes_max", "debut", "parentheses"}.
    """
    trouvees = []
    for m in _RE_ATOME.finditer(phrase):
        haut = _valeur(m["v"], m["u"], m["m"], bool(m["demi"]))
        bas, debut = haut, m.start()
        prefixe = _RE_DEBUT_PLAGE.search(phrase[: m.start()])
        if prefixe:
            unite_bas = prefixe["u"] or m["u"]
            bas = _valeur(prefixe["v"], unite_bas, prefixe["m"], False)
            debut = prefixe.start()
            # « 15 min - 20 min » : la première durée fait partie de la plage.
            trouvees = [d for d in trouvees if d["debut"] < debut and d["fin"] <= debut]
        trouvees.append({"bas": bas, "haut": haut, "debut": debut, "fin": m.end()})

    for m in _RE_SPECIAL.finditer(phrase):
        s = 900 if "quart" in m.group(0).lower() else 1800
        trouvees.append({"bas": s, "haut": s, "debut": m.start(), "fin": m.end()})
    trouvees.sort(key=lambda d: d["debut"])

    resultat = []
    for d in trouvees:
        bas, haut = sorted((d["bas"], d["haut"]))
        if bas <= 0 or bas > DUREE_MAX:
            continue
        avant = phrase[: d["debut"]]
        resultat.append({
            "secondes": bas,
            "secondes_max": haut if haut > bas and haut <= DUREE_MAX else None,
            "debut": d["debut"],
            "parentheses": avant.count("(") > avant.count(")"),
        })
    return resultat


def decouper_phrases(texte: str) -> list[str]:
    """Découpe un texte en phrases (sur . ! ? ; suivis d'un espace)."""
    return [p.strip() for p in re.split(r"(?<=[.!?;])\s+", texte) if p.strip()]


def formater_duree(secondes: int) -> str:
    """900 → « 15 min », 5400 → « 1 h 30 », 30 → « 30 s »."""
    if secondes < 60:
        return f"{secondes} s"
    if secondes < 3600:
        mn, s = divmod(secondes, 60)
        return f"{mn} min" + (f" {s} s" if s else "")
    h, reste = divmod(secondes, 3600)
    mn = reste // 60
    return f"{h} h" + (f" {mn:02d}" if mn else "")


def formater_plage(bas: int, haut: int | None) -> str:
    """Texte court d'une durée ou d'une plage : « 15-20 min », « 1 h-1 h 30 »."""
    if not haut:
        return formater_duree(bas)
    if haut < 60:
        return f"{bas}-{haut} s"
    if haut < 3600 and bas % 60 == 0 and haut % 60 == 0:
        return f"{bas // 60}-{haut // 60} min"
    return f"{formater_duree(bas)}-{formater_duree(haut)}"


# Contexte → début du libellé. L'ordre compte : le premier qui correspond gagne.
# Les motifs portent sur le texte sans accents et en minuscules.
_CONTEXTES = [
    (r"micro-?onde", "Micro-ondes"),
    (r"vapeur", "Vapeur"),
    (r"\bfour\b", "Four"),
    (r"congelateur", "Congélateur"),
    (r"refrigerateur|frigo", "Frigo"),
    (r"poele", "Poêle"),
    (r"casserole|bouillir|ebullition|bouillante|blanchir", "Casserole"),
    (r"refroidir|tiedir", "Refroidir"),
    (r"repos|reposer", "Repos"),
    (r"\blever\b|pousser|gonfler", "Levée"),
    (r"infuser", "Infusion"),
    (r"tremper", "Trempage"),
    (r"mariner", "Marinade"),
    (r"petrir", "Pétrir"),
    (r"\bmixer|\bmixez", "Mixer"),
    (r"fouetter|battre", "Fouetter"),
    (r"cuire|cuisson|dorer|griller|rotir|mijoter|saisir", "Cuisson"),
]


def _contexte(texte: str) -> str | None:
    t = sans_accents(texte)
    for motif, nom in _CONTEXTES:
        if re.search(motif, t):
            return nom
    return None


def _libelle(phrase: str, etape: str, indice: int, bas: int, haut: int | None) -> str:
    nom = _contexte(phrase) or _contexte(etape)
    temp = _RE_TEMPERATURE.search(phrase) or _RE_TEMPERATURE.search(etape)
    if temp and nom in (None, "Four", "Cuisson"):
        nom = f"Four {temp.group(1)}°C"
    if nom is None:
        nom = f"Étape {indice + 1}"
    return f"{nom} {formater_plage(bas, haut)}"


def libelle_minuteur(texte_etape: str, indice: int, bas: int, haut: int | None) -> str:
    """Libellé d'un minuteur dont la durée est connue par ailleurs (Mealie)."""
    return _libelle(texte_etape, texte_etape, indice, bas, haut)


def minuteurs_etape(texte: str, indice: int) -> list[dict]:
    """Minuteurs d'une étape : au plus un par phrase utile."""
    minuteurs = []
    for phrase in decouper_phrases(texte):
        if _RE_APARTE.search(sans_accents(phrase)):
            continue
        durees = extraire_durees(phrase)
        hors_parentheses = [d for d in durees if not d["parentheses"]]
        choix = (hors_parentheses or durees)[:1]
        for d in choix:
            if any(m["secondes"] == d["secondes"] for m in minuteurs):
                continue   # même durée rappelée plus loin dans l'étape
            minuteurs.append({
                "etape": indice,
                "secondes": d["secondes"],
                "secondes_max": d["secondes_max"],
                "libelle": _libelle(phrase, texte, indice, d["secondes"], d["secondes_max"]),
            })
    return minuteurs
