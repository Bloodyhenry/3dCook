"""Les 14 allergènes réglementaires (UE) et leur repérage dans le texte.

Dans Mealie, les allergènes d'une recette sont des étiquettes
« Allergène : Gluten », « Allergène : Œufs »… La passerelle s'en sert pour
marquer les lignes d'ingrédients concernées sur la 3DS.
L'export JSON crée ces étiquettes à partir des mots écrits en rouge.

Les motifs portent sur du texte sans accents et en minuscules.
"""

import re

from .texte import sans_accents

PREFIXE_TAG = "Allergène : "

# nom affiché → (motifs qui signalent l'allergène, motifs à ignorer avant)
_DEFINITIONS: dict[str, tuple[list[str], list[str]]] = {
    "Gluten": (
        [r"\bble\b", r"epeautre", r"seigle", r"\borge\b", r"avoine", r"kamut", r"gluten",
         r"chapelure", r"\bpains?\b", r"biscottes?", r"\bpates\b", r"nouilles", r"semoule",
         r"boulgour", r"couscous", r"muesli", r"grissinis?", r"feuilles? de bri(?:ck|que)s?",
         r"\bcoudes\b", r"fusill?is?", r"farine (?:semi-)?complete", r"farine integrale"],
        [r"pates? de fruits?", r"pates? d'amandes?"],
    ),
    "Œufs": ([r"\boeufs?\b"], []),
    "Lait": (
        [r"\blait\b", r"laitages?", r"beurre", r"fromages?", r"yaourts?", r"\bcremes?\b",
         r"mozzarella", r"parmesan", r"\bfeta\b", r"ricotta", r"mascarpone", r"\bcomte\b",
         r"chevre", r"brebis", r"\bvache\b", r"emmental", r"\bskyr\b", r"petits? suisses?",
         r"faisselle", r"gruyere", r"roquefort", r"gouda", r"cheddar", r"mimolette",
         r"saint[ -]nectaire", r"\banimal\b", r"lactose"],
        [r"lait (?:d'|de )(?:amandes?|avoine|epeautre|noisettes?|riz|coco|soja|chataigne|cajou)",
         r"laits? vegetaux|laits? vegetal", r"beurre de (?:cacahu[eo]tes?|amandes?|noisettes?|cajou|coco)",
         r"beurre vegetal", r"cremes? de (?:coco|soja|riz|marrons?|cajou|avoine)",
         r"yaourts? (?:vegetal|vegetaux|de soja|au soja|de coco)", r"fromages? vegetal"],
    ),
    "Fruits à coque": (
        [r"amandes?", r"noisettes?", r"\bnoix\b", r"cajou", r"pistaches?", r"pecan",
         r"macadamia", r"oleagineux", r"pignons?", r"fruits a coques?"],
        [r"noix de (?:coco|saint[ -]jacques?|muscade|beurre|veau)"],
    ),
    "Arachide": ([r"arachides?", r"cacahu[eo]tes?"], []),
    "Soja": ([r"\bsoja\b", r"\btofu\b", r"tempeh", r"\bmiso\b", r"tamari", r"edamames?", r"okara"], []),
    "Poisson": (
        [r"poissons?", r"saumon", r"\bthon\b", r"cabillaud", r"\bcolin\b", r"merlu", r"sardines?",
         r"maquereaux?", r"truites?", r"morue", r"\blieu\b", r"anchois", r"dorade", r"\bsole\b",
         r"merlan", r"haddock"],
        [],
    ),
    "Crustacés": ([r"crevettes?", r"crabes?", r"homards?", r"langoustines?", r"ecrevisses?",
                   r"gambas", r"fruits de mer"], []),
    "Mollusques": (
        [r"\bmoules?\b", r"calamars?", r"poulpes?", r"seiches?", r"huitres?",
         r"saint[ -]jacques?", r"coquillages?", r"fruits de mer"],
        [r"moules? (?:a|en|de) (?:muffins?|cakes?|silicone|gateaux?|tartelettes?|madeleines?)"],
    ),
    "Sésame": ([r"sesame", r"tahin[ei]?"], []),
    "Moutarde": ([r"moutarde"], []),
    "Céleri": ([r"celeri"], []),
    "Lupin": ([r"lupin"], []),
    "Sulfites": ([r"sulfites?", r"vinaigre balsamique"], []),
}

_COMPILES = {
    nom: (re.compile("|".join(motifs)), re.compile("|".join(exclus)) if exclus else None)
    for nom, (motifs, exclus) in _DEFINITIONS.items()
}
NOMS = list(_DEFINITIONS)
_PAR_CLE = {sans_accents(nom): nom for nom in NOMS}


def detecter(texte: str, parmi: set[str] | None = None) -> set[str]:
    """Allergènes mentionnés dans un texte (« 2 œufs » → {"Œufs"})."""
    t = sans_accents(texte)
    trouves = set()
    for nom, (motif, exclus) in _COMPILES.items():
        if parmi is not None and nom not in parmi:
            continue
        cible = exclus.sub(" ", t) if exclus else t
        if motif.search(cible):
            trouves.add(nom)
    return trouves


def nom_tag(allergene: str) -> str:
    return PREFIXE_TAG + allergene


def depuis_tags(noms_tags) -> set[str]:
    """Étiquettes Mealie → allergènes (insensible à la casse et aux accents).

    Mealie met des majuscules à chaque mot à l'import (« Fruits À Coque ») :
    on compare donc sans accents ni casse.
    """
    resultat = set()
    prefixe = sans_accents(PREFIXE_TAG).replace(" ", "")
    for nom in noms_tags:
        cle = sans_accents(nom).replace(" ", "")
        if cle.startswith(prefixe):
            reste = sans_accents(nom).split(":", 1)[-1].strip()
            if reste in _PAR_CLE:
                resultat.add(_PAR_CLE[reste])
    return resultat
