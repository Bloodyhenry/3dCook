"""Mise à l'échelle des quantités selon les portions.

C'est l'implémentation de référence : l'appli 3DS fait le calcul elle-même
(en C) et doit reproduire exactement ces règles, fixées par les tests.

Règles d'arrondi (pas d'arrondi selon l'unité et la valeur) :
  g, ml        : < 10 → 0,5 ; < 50 → 1 ; < 250 → 5 ; sinon 10
  kg, l        : < 1 → 0,05 ; sinon 0,1
  cl, dl       : < 10 → 0,5 ; sinon 1
  mg           : 1
  autres       : < 10 → ¼ (affiché ¼ ½ ¾) ; sinon 1
Une quantité positive n'est jamais arrondie à zéro (on garde le plus petit pas).
"""

import math

UNITES_METRIQUES = {"g", "kg", "mg", "ml", "cl", "dl", "l"}
# Unités qui ne prennent jamais de « s ».
UNITES_INVARIABLES = UNITES_METRIQUES | {"c. à soupe", "c. à café", "noix"}

_FRACTIONS = {0.25: "¼", 0.5: "½", 0.75: "¾"}


def facteur(portions_base: dict | None, valeur: float) -> float:
    """Coefficient à appliquer aux quantités.

    Si la recette connaît ses portions, `valeur` est le nombre voulu
    (ex. 6 → 9 ramequins donne 1,5). Sinon `valeur` est directement le
    multiplicateur choisi sur la console (×½, ×1, ×2…).
    """
    if portions_base and portions_base.get("nombre"):
        return valeur / portions_base["nombre"]
    return valeur


def pas_arrondi(q: float, unite: str | None) -> float:
    if unite in ("g", "ml"):
        return 0.5 if q < 10 else 1 if q < 50 else 5 if q < 250 else 10
    if unite in ("kg", "l"):
        return 0.05 if q < 1 else 0.1
    if unite in ("cl", "dl"):
        return 0.5 if q < 10 else 1
    if unite == "mg":
        return 1
    return 0.25 if q < 10 else 1


def arrondir(q: float, unite: str | None) -> float:
    """Arrondi « au demi supérieur » sur le pas de l'unité, jamais à zéro."""
    pas = pas_arrondi(q, unite)
    r = math.floor(q / pas + 0.5) * pas
    if r <= 0 < q:
        r = pas
    return round(r, 3)


def mettre_a_echelle(q: float | None, f: float, unite: str | None) -> float | None:
    if q is None:
        return None
    if f == 1:
        return q
    return arrondir(q * f, unite)


def formater_nombre(v: float, unite: str | None) -> str:
    """1.5 → « 1,5 » (métrique) ou « 1 ½ » (cuillères, pièces…)."""
    if v == int(v):
        return str(int(v))
    if unite not in UNITES_METRIQUES:
        entier, frac = int(v), round(v - int(v), 2)
        if frac in _FRACTIONS:
            return f"{entier} {_FRACTIONS[frac]}" if entier else _FRACTIONS[frac]
    return f"{v:.2f}".rstrip("0").rstrip(".").replace(".", ",")


def pluriel(unite: str, q: float) -> str:
    """« gousse » → « gousses » à partir de 2, « petit pot » → « petits pots »."""
    if unite in UNITES_INVARIABLES or q < 2:
        return unite
    mots = []
    for mot in unite.split(" "):
        if mot.endswith(("s", "x", "z")):
            mots.append(mot)
        elif mot.endswith(("eau", "au")):
            mots.append(mot + "x")
        elif mot.endswith("al"):
            mots.append(mot[:-2] + "aux")
        else:
            mots.append(mot + "s")
    return " ".join(mots)


def formater_ingredient(item: dict, f: float = 1.0) -> str:
    """Texte affiché pour un ingrédient, quantités mises à l'échelle."""
    q = mettre_a_echelle(item.get("quantite"), f, item.get("unite"))
    if q is None:
        return item["nom"]
    q2 = mettre_a_echelle(item.get("quantite_max"), f, item.get("unite"))
    unite = item.get("unite")
    texte = formater_nombre(q, unite)
    if q2:
        texte += "-" + formater_nombre(q2, unite)
    if unite:
        texte += " " + pluriel(unite, q2 or q)
    return f"{texte} {item['nom']}"
