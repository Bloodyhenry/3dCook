"""Tests de l'extraction des durées et des minuteurs."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.minuteurs import (  # noqa: E402
    extraire_durees, formater_duree, formater_plage, minuteurs_etape,
)


def premiere(texte):
    d = extraire_durees(texte)
    return (d[0]["secondes"], d[0]["secondes_max"]) if d else None


class TestDurees(unittest.TestCase):
    def test_formes_simples(self):
        cas = {
            "cuire 20 min": (1200, None),
            "cuire 20min": (1200, None),
            "cuire 20 minutes": (1200, None),
            "cuire 20 mn": (1200, None),
            "reposer 1 h": (3600, None),
            "reposer 2h": (7200, None),
            "reposer 2 heures": (7200, None),
            "mixer 30 secondes": (30, None),
            "mixer 30 s": (30, None),
            "laisser 1,5 h": (5400, None),
        }
        for texte, attendu in cas.items():
            with self.subTest(texte=texte):
                self.assertEqual(premiere(texte), attendu)

    def test_heures_et_minutes(self):
        self.assertEqual(premiere("reposer 1 h 30"), (5400, None))
        self.assertEqual(premiere("reposer 1h30"), (5400, None))
        self.assertEqual(premiere("reposer 1h30min"), (5400, None))
        self.assertEqual(premiere("cuire 1 heure et demie"), (5400, None))

    def test_plages(self):
        self.assertEqual(premiere("cuire 15-20min à 180°C"), (900, 1200))
        self.assertEqual(premiere("cuire 15 - 20 minutes"), (900, 1200))
        self.assertEqual(premiere("cuire 15 min - 20 min"), (900, 1200))
        self.assertEqual(premiere("cuire 20 à 30 min"), (1200, 1800))
        self.assertEqual(premiere("cuire entre 20 et 30 min"), (1200, 1800))
        self.assertEqual(premiere("au micro-ondes pendant 2 à 4 min"), (120, 240))
        self.assertEqual(premiere("lever 1h-1h30"), (3600, 5400))
        # Une seule durée pour « 15 min - 20 min », pas deux.
        self.assertEqual(len(extraire_durees("cuire 15 min - 20 min")), 1)

    def test_mots(self):
        self.assertEqual(premiere("laisser une heure"), (3600, None))
        self.assertEqual(premiere("cuire deux minutes"), (120, None))
        self.assertEqual(premiere("reposer une demi-heure"), (1800, None))
        self.assertEqual(premiere("reposer un quart d'heure"), (900, None))
        self.assertEqual(premiere("reposer 1/2 heure"), (1800, None))

    def test_pas_de_faux_positifs(self):
        for texte in [
            "cuire à 180°C",
            "ajouter 2 sachets de levure",
            "2 huiles différentes",
            "former 12 minces galettes",
            "se conserve 3 jours au réfrigérateur",
            "mettre 1 nuit au frais",
            "cuire quelques minutes",
            "ajouter 100 ml de lait",
        ]:
            with self.subTest(texte=texte):
                self.assertEqual(extraire_durees(texte), [])

    def test_plus_de_12_heures_ignore(self):
        self.assertEqual(extraire_durees("se garde 48h au frigo"), [])
        self.assertEqual(premiere("congeler au moins 12h"), (43200, None))

    def test_entre_minutes_et_nuit(self):
        # « 1 nuit » n'est pas une unité : il reste 30 min.
        self.assertEqual(premiere("au réfrigérateur entre 30 minutes et 1 nuit"), (1800, None))

    def test_parentheses_signalees(self):
        d = extraire_durees("cuire 20 min (ou 25 min pour les gros)")
        self.assertEqual([x["parentheses"] for x in d], [False, True])


class TestMinuteursEtape(unittest.TestCase):
    def test_four_avec_temperature(self):
        m = minuteurs_etape("Faire cuire au four 30min à 180°C.", 3)
        self.assertEqual(m, [{"etape": 3, "secondes": 1800, "secondes_max": None,
                              "libelle": "Four 180°C 30 min"}])

    def test_temperature_sans_le_mot_four(self):
        m = minuteurs_etape("Faire cuire à 200°C pendant 15-20min.", 5)
        self.assertEqual(m[0]["libelle"], "Four 200°C 15-20 min")
        self.assertEqual((m[0]["secondes"], m[0]["secondes_max"]), (900, 1200))

    def test_libelles_de_contexte(self):
        cas = {
            "Faire cuire le poisson à la vapeur 15-20min.": "Vapeur 15-20 min",
            "Mettre au réfrigérateur 2h.": "Frigo 2 h",
            "Laisser reposer 10 minutes.": "Repos 10 min",
            "Cuire au micro-ondes 2 min.": "Micro-ondes 2 min",
            "Laisser tremper 1 h 30.": "Trempage 1 h 30",
            "Patienter 5 min.": "Étape 3 5 min",
        }
        for texte, libelle in cas.items():
            with self.subTest(texte=texte):
                self.assertEqual(minuteurs_etape(texte, 2)[0]["libelle"], libelle)

    def test_une_duree_par_phrase(self):
        m = minuteurs_etape("Cuire 20 min (ou 25 min pour les gros).", 0)
        self.assertEqual([x["secondes"] for x in m], [1200])

    def test_parenthese_seule_retenue(self):
        m = minuteurs_etape("Cuire au four (20 min à 180°C).", 0)
        self.assertEqual([x["secondes"] for x in m], [1200])

    def test_deux_phrases_deux_minuteurs(self):
        m = minuteurs_etape("Cuire 10 min. Retourner et cuire encore 5 min.", 0)
        self.assertEqual([x["secondes"] for x in m], [600, 300])

    def test_apartes_ignores(self):
        texte = ("Faire cuire les biscuits environ 20min au four à 180°C. "
                 "Vous pouvez faire 5min de cuisson en plus pour les avoir croustillants.")
        self.assertEqual([x["secondes"] for x in minuteurs_etape(texte, 0)], [1200])

    def test_meme_duree_rappelee(self):
        texte = ("Laisser gélifier au moins 2h au réfrigérateur. "
                 "Ne pas bouger les flans pendant ces 2h.")
        self.assertEqual(len(minuteurs_etape(texte, 0)), 1)

    def test_congelation_dans_une_etape(self):
        m = minuteurs_etape("Mettre au congélateur pour 4-6h avant de servir.", 1)
        self.assertEqual(m[0]["libelle"], "Congélateur 4 h-6 h")


class TestFormatage(unittest.TestCase):
    def test_duree(self):
        self.assertEqual(formater_duree(30), "30 s")
        self.assertEqual(formater_duree(900), "15 min")
        self.assertEqual(formater_duree(90), "1 min 30 s")
        self.assertEqual(formater_duree(3600), "1 h")
        self.assertEqual(formater_duree(5400), "1 h 30")
        self.assertEqual(formater_duree(3900), "1 h 05")

    def test_plage(self):
        self.assertEqual(formater_plage(900, 1200), "15-20 min")
        self.assertEqual(formater_plage(900, None), "15 min")
        self.assertEqual(formater_plage(3600, 5400), "1 h-1 h 30")
        self.assertEqual(formater_plage(20, 30), "20-30 s")


if __name__ == "__main__":
    unittest.main()
