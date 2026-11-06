"""Tests de l'analyse des ingrédients, des portions et du nettoyage HTML."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.ingredients import (  # noqa: E402
    analyser_ingredients, analyser_ligne, detecter_portions, lire_nombre,
)
from recettes3ds.texte import html_vers_lignes, normaliser, sans_accents  # noqa: E402


def court(ligne):
    i = analyser_ligne(ligne)
    return (i["quantite"], i["quantite_max"], i["unite"], i["nom"])


class TestTexte(unittest.TestCase):
    def test_blocs_et_allergenes(self):
        h = ('<div>- 220 g de <font color="#FF0000">poisson</font> blanc</div>'
             '<div>- 1/2&nbsp;oignon<br></div><div><br></div>')
        self.assertEqual(html_vers_lignes(h), [
            ("- 220 g de poisson blanc", True),
            ("- 1/2 oignon", False),
        ])

    def test_variantes_de_rouge(self):
        for balise in ['<span style="color:red">', '<font color="#ff3300">',
                       '<span style="caret-color: rgb(255, 0, 0);">']:
            with self.subTest(balise=balise):
                self.assertTrue(html_vers_lignes(f"<div>2 {balise}œufs</span></div>")[0][1])

    def test_retours_a_la_ligne_bruts_sont_des_espaces(self):
        h = "<p><span>4) Faire\ncuire au four 30min à 180°C.</span></p>"
        self.assertEqual(html_vers_lignes(h)[0][0], "4) Faire cuire au four 30min à 180°C.")

    def test_typographie(self):
        self.assertEqual(normaliser("l’œuf⁠ …  ok !"), "l'œuf ... ok !")

    def test_sans_accents(self):
        self.assertEqual(sans_accents("Œufs brouillés à l'Été"), "oeufs brouilles a l'ete")


class TestNombres(unittest.TestCase):
    def test_formes(self):
        self.assertEqual(lire_nombre("12"), 12)
        self.assertEqual(lire_nombre("1,5"), 1.5)
        self.assertEqual(lire_nombre("1/2"), 0.5)
        self.assertEqual(lire_nombre("1 1/2"), 1.5)
        self.assertEqual(lire_nombre("½"), 0.5)
        self.assertEqual(lire_nombre("2½"), 2.5)
        self.assertEqual(lire_nombre("deux"), 2)
        self.assertIsNone(lire_nombre("1/0"))


class TestLigne(unittest.TestCase):
    def test_metrique(self):
        self.assertEqual(court("- 220 g de poisson blanc"), (220, None, "g", "de poisson blanc"))
        self.assertEqual(court("- 135g de farine de blé T60"), (135, None, "g", "de farine de blé T60"))
        self.assertEqual(court("- 100ml de lait d'amande"), (100, None, "ml", "de lait d'amande"))
        self.assertEqual(court("1 kg de pommes"), (1, None, "kg", "de pommes"))
        self.assertEqual(court("25 cl de crème"), (25, None, "cl", "de crème"))

    def test_cuilleres(self):
        attendu = (3, None, "c. à soupe", "d'huile d'olive")
        self.assertEqual(court("- 3 cuillères à soupe d'huile d'olive"), attendu)
        self.assertEqual(court("- 3 c. à s. d'huile d'olive"), attendu)
        self.assertEqual(court("- 3 CS d'huile d'olive"), attendu)
        self.assertEqual(court("- 2 grosses cuillères à soupe de purée"),
                         (2, None, "c. à soupe", "de purée"))
        self.assertEqual(court("- 1 cuillère à café de poudre à lever"),
                         (1, None, "c. à café", "de poudre à lever"))

    def test_pieces_et_fractions(self):
        self.assertEqual(court("- 1/2 oignon"), (0.5, None, None, "oignon"))
        self.assertEqual(court("- 2 œufs"), (2, None, None, "œufs"))
        self.assertEqual(court("- 3 gousses d'ail"), (3, None, "gousse", "d'ail"))
        self.assertEqual(court("- 1 sachet de poudre à lever"), (1, None, "sachet", "de poudre à lever"))
        self.assertEqual(court("- une pincée de sel"), (1, None, "pincée", "de sel"))

    def test_plages(self):
        self.assertEqual(court("- 80-100g de chocolat"), (80, 100, "g", "de chocolat"))
        self.assertEqual(court("- 2 à 3 bananes"), (2, 3, None, "bananes"))

    def test_unite_mot_entier(self):
        # « gros » commence par « g » mais n'est pas l'unité gramme.
        self.assertEqual(court("- 2 gros œufs"), (2, None, None, "gros œufs"))
        self.assertEqual(court("- 4 grammes de sel"), (4, None, "g", "de sel"))

    def test_sans_quantite(self):
        self.assertEqual(court("- persil"), (None, None, None, "persil"))
        self.assertEqual(court("- optionnel : épices"), (None, None, None, "optionnel : épices"))
        self.assertEqual(court("- quelques fraises"), (None, None, None, "quelques fraises"))
        # « 100% cacao » n'est pas une quantité.
        self.assertEqual(court("100% cacao"), (None, None, None, "100% cacao"))

    def test_allergene_conserve(self):
        self.assertTrue(analyser_ligne("- 2 œufs", allergene=True)["allergene"])


class TestGroupes(unittest.TestCase):
    def test_titres_de_groupe(self):
        lignes = [("Pour la pâte :", False), ("- 200 g de farine", False),
                  ("Pour la garniture :", False), ("- 2 pommes", False)]
        g = analyser_ingredients(lignes)
        self.assertEqual([x["titre"] for x in g], ["Pour la pâte", "Pour la garniture"])
        self.assertEqual([len(x["items"]) for x in g], [1, 1])

    def test_sans_titre(self):
        g = analyser_ingredients([("- 2 pommes", False)])
        self.assertEqual(g[0]["titre"], None)

    def test_liste_vide(self):
        self.assertEqual(analyser_ingredients([]), [{"titre": None, "items": []}])


class TestPortions(unittest.TestCase):
    def test_detections(self):
        cas = {
            "Pour 6 ramequins :": {"nombre": 6, "unite": "ramequins"},
            "Pour 12 petites crêpes :": {"nombre": 12, "unite": "petites crêpes"},
            "Pour 8 moules à muffins :": {"nombre": 8, "unite": "moules à muffins"},
            "Pour 10 à 12 cookies": {"nombre": 10, "unite": "cookies"},
            "On obtient environ 15 muffins.": {"nombre": 15, "unite": "muffins"},
        }
        for texte, attendu in cas.items():
            with self.subTest(texte=texte):
                self.assertEqual(detecter_portions(texte), attendu)

    def test_rejets(self):
        for texte in [
            "cuire environ 10 min",
            "340 g de banane (environ 4 bananes)",
            "pour 2 cm de diamètre",
            "pour 3 jours au frais",
            "pour 1 l de lait",
        ]:
            with self.subTest(texte=texte):
                self.assertIsNone(detecter_portions(texte))

    def test_ordre_des_textes(self):
        self.assertEqual(detecter_portions("rien", "Pour 4 personnes")["nombre"], 4)


if __name__ == "__main__":
    unittest.main()
