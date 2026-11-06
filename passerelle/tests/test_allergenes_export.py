"""Tests des allergènes et de l'export schema.org (archive pour Mealie)."""

import json
import shutil
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "outils"))

from exporter_mealie import exporter, noms_uniques, slug  # noqa: E402
from recettes3ds import allergenes  # noqa: E402
from recettes3ds.sources.export_json import vers_schemaorg  # noqa: E402
from recettes3ds.texte import segments_rouges  # noqa: E402


class TestAllergenes(unittest.TestCase):
    def test_detection(self):
        cas = {
            "farine de blé T80": {"Gluten"},
            "2 œufs": {"Œufs"},
            "noix de cajou": {"Fruits à coque"},
            "lait d'amande": {"Fruits à coque"},          # pas « Lait »
            "lait de coco": set(),
            "noix de coco râpée": set(),
            "une noix de beurre": {"Lait"},
            "beurre de cacahuète": {"Arachide"},
            "fromage blanc de brebis": {"Lait"},
            "moules à muffins": set(),
            "200 g de moules": {"Mollusques"},
            "tofu soyeux": {"Soja"},
            "crème de marrons": set(),
            "pain dur": {"Gluten"},
            "blettes": set(),
            "graines de sésame": {"Sésame"},
            "noix de saint jacques": {"Mollusques"},
        }
        for texte, attendu in cas.items():
            with self.subTest(texte=texte):
                self.assertEqual(allergenes.detecter(texte), attendu)

    def test_restriction(self):
        self.assertEqual(allergenes.detecter("pain aux œufs", parmi={"Œufs"}), {"Œufs"})

    def test_tags_mealie(self):
        # Mealie met une majuscule à chaque mot à l'import.
        tags = ["Import", "Allergène : Gluten", "Allergène : Fruits À Coque",
                "allergene : oeufs", "Allergène : Inconnu", "Congelable"]
        self.assertEqual(allergenes.depuis_tags(tags), {"Gluten", "Fruits à coque", "Œufs"})

    def test_segments_rouges(self):
        h = ('<div>220 g de <font color="#FF0000">poisson</font> blanc</div>'
             "<div>100ml de lait d'<span style=\"color:red\">amande</span></div>"
             '<div><font color="#ff0000"> - </font></div>')
        self.assertEqual(segments_rouges(h), ["poisson", "amande"])


SOURCE = {
    "id": "12",
    "title": "Tarte test",
    "image": "https://exemple.invalid/images/recettes/tarte.jpg",
    "hasCongelation": True,
    "conseil": "<div>Un conseil.</div>",
    "ingredientsHtml": ("<div>Pour la pâte :</div>"
                        '<div>- 200 g de farine de <font color="#FF0000">blé</font></div>'
                        "<div>- 1 pincée de sel</div>"
                        "<div>Pour la garniture :</div>"
                        '<div>- 2 <font color="#FF0000">œufs</font></div>'),
    "prepHtml": ("<div>Pour la pâte :</div><div>1) Mélanger,</div><div>2) Étaler.</div>"
                 "<div>Pour la garniture :</div><div>3) Battre les œufs.</div>"
                 "<div>4) Cuire 30 min à 180°C.</div>"
                 "<div>La tarte se conserve 2 jours.</div>"),
    "url": "https://exemple.invalid/recettes/12",
}


class TestSchemaOrg(unittest.TestCase):
    def test_structure(self):
        r = vers_schemaorg(SOURCE)
        self.assertEqual(r["@type"], "Recipe")
        self.assertEqual(r["name"], "Tarte test")
        self.assertEqual(r["description"], "Un conseil.")
        self.assertEqual(r["url"], SOURCE["url"])
        self.assertEqual(r["keywords"], "Congelable,Allergène : Gluten,Allergène : Œufs")
        self.assertEqual(r["notes"], ["La tarte se conserve 2 jours."])
        self.assertNotIn("recipeYield", r)

    def test_etiquette(self):
        r = vers_schemaorg(SOURCE, etiquette="Import")
        self.assertEqual(r["keywords"], "Import,Congelable,Allergène : Gluten,Allergène : Œufs")

    def test_ingredients_avec_titres(self):
        # Mealie n'accepte que des chaînes OU que des objets : tout est objet.
        ingr = vers_schemaorg(SOURCE)["recipeIngredient"]
        self.assertTrue(all(isinstance(i, dict) for i in ingr))
        self.assertEqual(ingr, [
            {"note": "200 g de farine de blé", "title": "Pour la pâte"},
            {"note": "1 pincée de sel"},
            {"note": "2 œufs", "title": "Pour la garniture"},
        ])

    def test_sections_d_etapes(self):
        instr = vers_schemaorg(SOURCE)["recipeInstructions"]
        self.assertEqual([x["@type"] for x in instr], ["HowToSection", "HowToSection"])
        self.assertEqual(instr[0]["name"], "Pour la pâte")
        self.assertEqual([p["text"] for p in instr[0]["itemListElement"]], ["Mélanger.", "Étaler."])
        self.assertEqual([p["text"] for p in instr[1]["itemListElement"]],
                         ["Battre les œufs.", "Cuire 30 min à 180°C."])

    def test_portions(self):
        source = dict(SOURCE, ingredientsHtml="<div>Pour 6 ramequins :</div><div>- 2 œufs</div>")
        self.assertEqual(vers_schemaorg(source)["recipeYield"], "6 ramequins")

    def test_noms(self):
        self.assertEqual(slug("Œufs brouillés à l'été !"), "oeufs-brouilles-a-l-ete")
        self.assertEqual(noms_uniques(["Cake", "cake", "Tarte", "Cake"]),
                         ["Cake", "cake (2)", "Tarte", "Cake (3)"])


class TestArchive(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.source = self.tmp / "base"
        (self.source / "images").mkdir(parents=True)
        (self.source / "base.json").write_text(json.dumps([SOURCE, dict(SOURCE, id="13", image="")]),
                                               encoding="utf-8")
        Image.new("RGB", (50, 50)).save(self.source / "images" / "tarte.jpg")

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def test_archive_nextcloud(self):
        sortie = self.tmp / "export.zip"
        bilan = exporter(self.source, "base.json", sortie)
        self.assertEqual((bilan["recettes"], bilan["photos"]), (2, 1))
        with zipfile.ZipFile(sortie) as zf:
            noms = sorted(zf.namelist())
            self.assertEqual(noms, [
                "recettes/12-tarte-test/full.jpg",
                "recettes/12-tarte-test/recipe.json",
                "recettes/13-tarte-test-2/recipe.json",
            ])
            r = json.loads(zf.read("recettes/13-tarte-test-2/recipe.json"))
            self.assertEqual(r["name"], "Tarte test (2)")

    def test_refuse_d_ecrire_dans_la_base(self):
        with self.assertRaises(SystemExit):
            exporter(self.source, "base.json", self.source / "export.zip")


if __name__ == "__main__":
    unittest.main()
