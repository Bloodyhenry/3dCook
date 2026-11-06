"""Tests du découpage de la préparation en étapes et notes."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.etapes import analyser_etapes, nettoyer_etape  # noqa: E402
from recettes3ds.texte import html_vers_lignes  # noqa: E402


def etapes_de(html):
    etapes, notes = analyser_etapes(html_vers_lignes(html))
    return [e["texte"] for e in etapes], notes


class TestNettoyage(unittest.TestCase):
    def test_ponctuation(self):
        self.assertEqual(nettoyer_etape("mixer le poisson,"), "Mixer le poisson.")
        self.assertEqual(nettoyer_etape("Cuire 20 min ;"), "Cuire 20 min.")
        self.assertEqual(nettoyer_etape("Servir !"), "Servir !")
        self.assertEqual(nettoyer_etape("cuire (à couvert)"), "Cuire (à couvert)")
        self.assertEqual(nettoyer_etape("  ,  "), "")


class TestNumerotees(unittest.TestCase):
    def test_acras(self):
        html = ("<div>1)&nbsp;&nbsp;&nbsp; Faire cuire le poisson à la vapeur 15-20min,</div>"
                "<div>2)&nbsp;&nbsp;&nbsp; Mixer le poisson avec l'oignon,</div>"
                "<div>3)&nbsp;&nbsp;&nbsp; Faire cuire à 200°C pendant 15-20min.</div>"
                "<div><br></div><div>Les acras se conservent 3 jours au réfrigérateur.<br></div>")
        etapes, notes = etapes_de(html)
        self.assertEqual(etapes, [
            "Faire cuire le poisson à la vapeur 15-20min.",
            "Mixer le poisson avec l'oignon.",
            "Faire cuire à 200°C pendant 15-20min.",
        ])
        self.assertEqual(notes, ["Les acras se conservent 3 jours au réfrigérateur."])

    def test_numero_seul_sur_sa_ligne(self):
        html = "<p>1)</p><p>Mélanger les aliments secs,</p><p>2) Cuire.</p>"
        self.assertEqual(etapes_de(html)[0], ["Mélanger les aliments secs.", "Cuire."])

    def test_suite_en_minuscule(self):
        html = "<div>1) Faire</div><div>cuire au four 50min à 180°C.</div>"
        self.assertEqual(etapes_de(html)[0], ["Faire cuire au four 50min à 180°C."])

    def test_ligne_entre_deux_etapes(self):
        html = ("<div>1) Mélanger.</div><div>Laisser reposer.</div>"
                "<div>2) Cuire.</div>")
        self.assertEqual(etapes_de(html)[0], ["Mélanger. Laisser reposer.", "Cuire."])

    def test_sections(self):
        html = ("<div>Pour la pâte :</div><div>1) Mélanger.</div>"
                "<div>Pour la garniture :</div><div>2) Couper.</div><div>3) Garnir.</div>")
        etapes, _ = analyser_etapes(html_vers_lignes(html))
        self.assertEqual([e.get("section") for e in etapes],
                         ["Pour la pâte", "Pour la garniture", None])

    def test_consigne_avant_la_premiere_etape(self):
        html = "<div>Préchauffer le four à 180°C.</div><div>1) Mélanger.</div>"
        self.assertEqual(etapes_de(html)[0], ["Préchauffer le four à 180°C.", "Mélanger."])

    def test_quantite_decimale_pas_un_numero(self):
        html = "<div>1) Ajouter</div><div>1.5 kg de pommes.</div>"
        self.assertEqual(etapes_de(html)[0], ["Ajouter 1.5 kg de pommes."])


class TestTexteLibre(unittest.TestCase):
    def test_phrases(self):
        html = ("<div>Il suffit de tout mixer ensemble et de former des boules avec les doigts. "
                "Ensuite il faut les écraser et les faire cuire au four 20 min. "
                "Enfin on peut garnir les pains burger avec la salade.</div>"
                "<div>Les steaks se conservent 3 jours au réfrigérateur.</div>")
        etapes, notes = etapes_de(html)
        self.assertEqual(len(etapes), 3)
        self.assertTrue(etapes[1].startswith("Ensuite"))
        self.assertEqual(notes, ["Les steaks se conservent 3 jours au réfrigérateur."])

    def test_phrases_courtes_collees(self):
        html = "<div>Mixer. Cuire. Servir chaud avec une salade verte bien assaisonnée.</div>"
        self.assertEqual(etapes_de(html)[0],
                         ["Mixer. Cuire. Servir chaud avec une salade verte bien assaisonnée."])

    def test_jamais_sans_etape(self):
        html = "<div>Verser le yaourt dans un moule et faire congeler 4h.</div>"
        etapes, notes = etapes_de(html)
        self.assertEqual(len(etapes), 1)
        self.assertEqual(notes, [])


if __name__ == "__main__":
    unittest.main()
