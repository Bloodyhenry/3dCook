"""Découpage de la préparation en étapes courtes et en notes.

Deux cas dans la base :
  - 86 % des recettes numérotent leurs étapes (« 1) Mélanger… ») ;
  - les autres sont en texte libre, qu'on découpe par phrase.
Les lignes de conservation (« Se conserve 3 jours… ») deviennent des notes.
Un titre court terminé par « : » (« Pour la sauce : ») devient la « section »
de l'étape suivante.
"""

import re

from .texte import sans_accents

_RE_NUMERO = re.compile(r"^\s*(\d{1,2})\s*[).]\s*(?!\d)")
_RE_NOTE = re.compile(
    r"conserv|congel|se garde|bon appetit|astuce|se degust|peuvent se|peut se|"
    r"^(?:vous pouvez|on peut|il est possible|n'hesitez|possibilite)"
)
_RE_PUCE = re.compile(r"^[-*•·]+\s*")
# En dessous de cette longueur, une phrase est collée à la suivante.
_PHRASE_MIN = 40


def _est_titre(ligne: str) -> bool:
    return ligne.endswith(":") and len(ligne) <= 60


def _est_note(ligne: str) -> bool:
    return bool(_RE_NOTE.search(sans_accents(ligne)))


def nettoyer_etape(texte: str) -> str:
    """Majuscule initiale, ponctuation finale propre (« , » → « . »)."""
    texte = _RE_PUCE.sub("", texte).strip()
    texte = re.sub(r"\s+([,.])", r"\1", texte)
    texte = re.sub(r"[,;:\s]+$", "", texte)
    if not texte:
        return ""
    if texte[-1] not in ".!?)":
        texte += "."
    return texte[0].upper() + texte[1:]


def _decouper_phrases(texte: str) -> list[str]:
    """Phrases d'un paragraphe, les très courtes collées à la suivante."""
    morceaux = [p for p in re.split(r"(?<=[.!?])\s+(?=[A-ZÀ-ÖØ-Ý])", texte) if p.strip()]
    phrases, tampon = [], ""
    for m in morceaux:
        tampon = f"{tampon} {m}".strip()
        if len(tampon) >= _PHRASE_MIN:
            phrases.append(tampon)
            tampon = ""
    if tampon:
        if phrases:
            phrases[-1] += " " + tampon
        else:
            phrases.append(tampon)
    return phrases


def _ajouter(etapes: list[dict], texte: str, section: str | None) -> None:
    propre = nettoyer_etape(texte)
    if propre:
        etape = {"texte": propre}
        if section:
            etape["section"] = section
        etapes.append(etape)


def _numerotees(lignes: list[str]) -> tuple[list[dict], list[str]]:
    brutes: list[dict] = []        # étapes en cours de construction
    en_attente: list[str] = []     # lignes non numérotées après une étape
    notes: list[str] = []
    section = None

    for ligne in lignes:
        m = _RE_NUMERO.match(ligne)
        if m:
            # Des lignes entre deux étapes numérotées complètent la précédente.
            if brutes and en_attente:
                brutes[-1]["texte"] += " " + " ".join(en_attente)
            en_attente = []
            brutes.append({"texte": ligne[m.end():].strip(), "section": section})
            section = None
        elif _est_titre(ligne):
            section = ligne.rstrip(" :")
        elif not brutes:
            # Avant la première étape : consigne générale ou note.
            if _est_note(ligne):
                notes.append(ligne)
            else:
                brutes.append({"texte": ligne, "section": section})
                section = None
        else:
            precedente = brutes[-1]["texte"]
            suite_evidente = (
                not precedente
                or ligne[0].islower()
                or (precedente[-1] not in ".!?" and not _est_note(ligne))
            )
            if suite_evidente and not en_attente:
                brutes[-1]["texte"] = f"{precedente} {ligne}".strip()
            else:
                en_attente.append(ligne)

    # Les lignes qui suivent la dernière étape sont des notes.
    notes.extend(en_attente)
    etapes: list[dict] = []
    for b in brutes:
        _ajouter(etapes, b["texte"], b["section"])
    return etapes, notes


def _libres(lignes: list[str]) -> tuple[list[dict], list[str]]:
    etapes: list[dict] = []
    notes: list[str] = []
    section = None
    for ligne in lignes:
        if _est_titre(ligne):
            section = ligne.rstrip(" :")
            continue
        for phrase in _decouper_phrases(ligne):
            if _est_note(phrase):
                notes.append(phrase)
            else:
                _ajouter(etapes, phrase, section)
                section = None
    return etapes, notes


def analyser_etapes(lignes: list[tuple[str, bool]]) -> tuple[list[dict], list[str]]:
    """Renvoie (étapes, notes). Chaque étape : {"texte", ["section"]}."""
    textes = [_RE_PUCE.sub("", t).strip() for t, _ in lignes]
    textes = [t for t in textes if t]
    if any(_RE_NUMERO.match(t) for t in textes):
        etapes, notes = _numerotees(textes)
    else:
        etapes, notes = _libres(textes)
    if not etapes:
        # Préparation réduite à une phrase « note » : on la garde comme étape.
        etapes, notes = [{"texte": n} for n in notes], []
    return etapes, [n for n in (nettoyer_etape(n) for n in notes) if n]
