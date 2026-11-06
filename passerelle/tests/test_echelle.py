"""Tests de la mise à l'échelle des portions (règles que la 3DS reproduira en C)."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.echelle import (  # noqa: E402
    arrondir, facteur, formater_ingredient, formater_nombre, mettre_a_echelle, pluriel,
)


def item(q, unite, nom, q2=None):
    return {"quantite": q, "quantite_max": q2, "unite": unite, "nom": nom, "allergene": False}


class TestFacteur(unittest.TestCase):
    def test_portions_connues(self):
        base = {"nombre": 6, "unite": "ramequins"}
        self.assertEqual(facteur(base, 9), 1.5)
        self.assertEqual(facteur(base, 3), 0.5)
        self.assertEqual(facteur(base, 6), 1)

    def test_portions_inconnues_multiplicateur(self):
        self.assertEqual(facteur(None, 2), 2)
        self.assertEqual(facteur(None, 0.5), 0.5)


class TestArrondi(unittest.TestCase):
    def test_grammes(self):
        self.assertEqual(arrondir(3.3, "g"), 3.5)      # pas 0,5 sous 10 g
        self.assertEqual(arrondir(33.4, "g"), 33)      # pas 1 sous 50 g
        self.assertEqual(arrondir(67.5, "g"), 70)      # pas 5 sous 250 g
        self.assertEqual(arrondir(66, "g"), 65)
        self.assertEqual(arrondir(333, "g"), 330)      # pas 10 au-delà
        self.assertEqual(arrondir(335, "g"), 340)      # demi → au-dessus

    def test_litres_et_cl(self):
        self.assertEqual(arrondir(0.33, "l"), 0.35)
        self.assertEqual(arrondir(1.26, "l"), 1.3)
        self.assertEqual(arrondir(12.4, "cl"), 12)

    def test_pieces_au_quart(self):
        self.assertEqual(arrondir(0.33, None), 0.25)
        self.assertEqual(arrondir(1.4, None), 1.5)
        self.assertEqual(arrondir(0.1, "c. à café"), 0.25)   # jamais zéro
        self.assertEqual(arrondir(13.4, None), 13)

    def test_facteur_un_ne_touche_rien(self):
        self.assertEqual(mettre_a_echelle(135, 1, "g"), 135)
        self.assertEqual(mettre_a_echelle(0.33, 1, None), 0.33)
        self.assertIsNone(mettre_a_echelle(None, 2, None))


class TestFormatage(unittest.TestCase):
    def test_nombres(self):
        self.assertEqual(formater_nombre(2, "g"), "2")
        self.assertEqual(formater_nombre(1.5, "l"), "1,5")
        self.assertEqual(formater_nombre(0.35, "l"), "0,35")
        self.assertEqual(formater_nombre(1.5, None), "1 ½")
        self.assertEqual(formater_nombre(0.25, "c. à café"), "¼")
        self.assertEqual(formater_nombre(2.75, None), "2 ¾")

    def test_pluriel(self):
        self.assertEqual(pluriel("gousse", 1), "gousse")
        self.assertEqual(pluriel("gousse", 1.5), "gousse")
        self.assertEqual(pluriel("gousse", 2), "gousses")
        self.assertEqual(pluriel("petit pot", 3), "petits pots")
        self.assertEqual(pluriel("morceau", 2), "morceaux")
        self.assertEqual(pluriel("bocal", 2), "bocaux")
        self.assertEqual(pluriel("c. à soupe", 3), "c. à soupe")
        self.assertEqual(pluriel("g", 200), "g")

    def test_ingredients_complets(self):
        self.assertEqual(formater_ingredient(item(220, "g", "de poisson blanc"), 2),
                         "440 g de poisson blanc")
        self.assertEqual(formater_ingredient(item(0.5, None, "oignon"), 3), "1 ½ oignon")
        self.assertEqual(formater_ingredient(item(1, "gousse", "d'ail"), 2), "2 gousses d'ail")
        self.assertEqual(formater_ingredient(item(80, "g", "de chocolat", 100), 0.5),
                         "40-50 g de chocolat")
        self.assertEqual(formater_ingredient(item(None, None, "persil"), 2), "persil")
        self.assertEqual(formater_ingredient(item(135, "g", "de farine"), 1), "135 g de farine")


if __name__ == "__main__":
    unittest.main()
