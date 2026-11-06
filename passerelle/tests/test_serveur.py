"""Tests de bout en bout : conversion, serveur HTTP, lecture seule de la base."""

import hashlib
import json
import shutil
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.construire import calculer_rev  # noqa: E402
from recettes3ds.sources.export_json import SourceExportJson  # noqa: E402
from recettes3ds.sources.export_json import convertir_export as convertir  # noqa: E402
from recettes3ds.serveur import creer_serveur  # noqa: E402

ACRAS = {
    "id": "416",
    "title": "Acras de poisson",
    "image": "https://exemple.invalid/images/recettes/acras.jpg",
    "hasCongelation": True,
    "conseil": "<div>Les acras sont des petits beignets de poisson.</div>",
    "ingredientsHtml": ('<div>- 220 g de <font color="#FF0000">poisson</font> blanc</div>'
                        "<div>- 1/2 oignon</div><div>- persil</div>"),
    "prepHtml": ("<div>1) Faire cuire le poisson à la vapeur 15-20min,</div>"
                 "<div>2) Mixer le poisson avec l'oignon,</div>"
                 "<div>3) Faire cuire à 200°C pendant 15-20min.</div>"
                 "<div>Les acras se conservent 3 jours au réfrigérateur.</div>"),
    "url": "https://exemple.invalid/recettes/416",
}
CAKE = {
    "id": "388", "title": "Cake coco", "image": "", "hasCongelation": False, "conseil": "",
    "ingredientsHtml": "<div>Pour 6 muffins :</div><div>- 2 œufs</div>",
    "prepHtml": "<p>1) Mélanger,</p><p>2) Cuire 30min à 180°C.</p>",
    "url": "",
}


def empreinte_dossier(dossier: Path) -> dict:
    """Contenu et date de chaque fichier : sert à vérifier la lecture seule."""
    return {str(p.relative_to(dossier)): (hashlib.sha1(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns)
            for p in sorted(dossier.rglob("*")) if p.is_file()}


class TestConversion(unittest.TestCase):
    def test_acras(self):
        r = convertir(ACRAS, image_presente=True)
        self.assertEqual(r["v"], 1)
        self.assertEqual(r["id"], "416")
        self.assertEqual(r["titre"], "Acras de poisson")
        self.assertIsNone(r["portions"])
        self.assertTrue(r["congelable"])
        items = r["groupes"][0]["items"]
        self.assertEqual(items[0], {"quantite": 220, "quantite_max": None, "unite": "g",
                                    "nom": "de poisson blanc", "allergene": True})
        self.assertEqual(len(r["etapes"]), 3)
        self.assertEqual([(m["etape"], m["secondes"], m["secondes_max"], m["libelle"])
                          for m in r["minuteurs"]],
                         [(0, 900, 1200, "Vapeur 15-20 min"), (2, 900, 1200, "Four 200°C 15-20 min")])
        self.assertEqual(r["notes"], ["Les acras se conservent 3 jours au réfrigérateur."])
        self.assertEqual(r["conseil"], "Les acras sont des petits beignets de poisson.")
        self.assertEqual(r["rev"], calculer_rev(r))

    def test_correction_prime_sur_la_detection(self):
        self.assertEqual(convertir(CAKE)["portions"], {"nombre": 6, "unite": "muffins"})
        corr = {"portions": {"nombre": 12, "unite": "minis"}}
        r = convertir(CAKE, correction=corr)
        self.assertEqual(r["portions"], {"nombre": 12, "unite": "minis"})
        self.assertNotEqual(r["rev"], convertir(CAKE)["rev"])

    def test_rev_stable(self):
        self.assertEqual(convertir(ACRAS)["rev"], convertir(ACRAS)["rev"])


class TestServeur(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp())
        cls.source = cls.tmp / "source"
        (cls.source / "images").mkdir(parents=True)
        (cls.source / "base.json").write_text(json.dumps([ACRAS, CAKE], ensure_ascii=False),
                                              encoding="utf-8")
        # Photo plus grande que l'écran pour vérifier la réduction.
        Image.new("RGB", (1200, 900), (200, 120, 40)).save(cls.source / "images" / "acras.jpg")
        cls.empreinte = empreinte_dossier(cls.source)
        cls.source.chmod(0o555)   # comme le montage :ro de Docker

        cls.source_h = SourceExportJson(cls.source, "base.json", "images", cls.tmp / "data")
        cls.serveur = creer_serveur(cls.source_h, "127.0.0.1", 0)
        cls.url = f"http://127.0.0.1:{cls.serveur.server_address[1]}"
        threading.Thread(target=cls.serveur.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.serveur.shutdown()
        cls.serveur.server_close()
        cls.source.chmod(0o755)
        shutil.rmtree(cls.tmp)

    def requete(self, methode, chemin, corps=None):
        donnees = json.dumps(corps).encode() if corps is not None else None
        req = urllib.request.Request(self.url + chemin, data=donnees, method=methode)
        if donnees:
            req.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(req, timeout=5) as rep:
                return rep.status, rep.headers, rep.read()
        except urllib.error.HTTPError as e:
            return e.code, e.headers, e.read()

    def json(self, methode, chemin, corps=None):
        code, entetes, brut = self.requete(methode, chemin, corps)
        if code == 200:
            self.assertEqual(entetes["Content-Type"], "application/json; charset=utf-8")
        return code, json.loads(brut.decode("utf-8"))

    def test_1_menu_vide(self):
        code, menu = self.json("GET", "/menu")
        self.assertEqual(code, 200)
        self.assertEqual(menu["recettes"], [])

    def test_2_ajout_et_retrait(self):
        code, menu = self.json("PUT", "/api/menu/416")
        self.assertEqual(code, 200)
        self.assertEqual([r["id"] for r in menu["recettes"]], ["416"])
        self.assertEqual(set(menu["recettes"][0]), {"id", "titre", "rev", "image"})
        self.json("PUT", "/api/menu/388")
        self.json("PUT", "/api/menu/388")   # deux fois : pas de doublon
        code, menu = self.json("GET", "/menu")
        self.assertEqual([r["id"] for r in menu["recettes"]], ["416", "388"])
        code, menu = self.json("DELETE", "/api/menu/388")
        self.assertEqual([r["id"] for r in menu["recettes"]], ["416"])
        # Le menu survit au redémarrage (fichier dans le dossier de données).
        relu = SourceExportJson(self.source, "base.json", "images", self.tmp / "data")
        self.assertEqual(relu.etat.ids_menu(), ["416"])

    def test_recette(self):
        code, r = self.json("GET", "/recipes/416")
        self.assertEqual(code, 200)
        self.assertEqual(r["titre"], "Acras de poisson")
        self.assertTrue(r["image"])
        _, r2 = self.json("GET", "/recipes/388")
        self.assertFalse(r2["image"])

    def test_utf8(self):
        _, _, brut = self.requete("GET", "/recipes/388")
        self.assertIn("œufs".encode("utf-8"), brut)

    def test_image_reduite(self):
        code, entetes, brut = self.requete("GET", "/recipes/416/image")
        self.assertEqual(code, 200)
        self.assertEqual(entetes["Content-Type"], "image/jpeg")
        chemin = self.tmp / "verif.jpg"
        chemin.write_bytes(brut)
        with Image.open(chemin) as img:
            self.assertLessEqual(img.width, 400)
            self.assertLessEqual(img.height, 240)
            self.assertEqual(img.size, (320, 240))
            self.assertFalse(img.info.get("progressive"))

    def test_sans_image(self):
        code, _ = self.json("GET", "/recipes/388/image")
        self.assertEqual(code, 404)

    def test_erreurs(self):
        self.assertEqual(self.json("GET", "/recipes/999")[0], 404)
        self.assertEqual(self.json("GET", "/recipes/..%2F..%2Fetc")[0], 400)
        self.assertEqual(self.json("GET", "/nimporte")[0], 404)
        self.assertEqual(self.json("PUT", "/api/menu/999")[0], 404)

    def test_correction_portions(self):
        code, r = self.json("PUT", "/api/corrections/416", {"portions": {"nombre": 15, "unite": "acras"}})
        self.assertEqual(code, 200)
        self.assertEqual(r["portions"], {"nombre": 15, "unite": "acras"})
        _, r = self.json("GET", "/recipes/416")
        self.assertEqual(r["portions"]["nombre"], 15)
        code, r = self.json("PUT", "/api/corrections/416", {"portions": None})
        self.assertIsNone(r["portions"])
        self.assertEqual(self.json("PUT", "/api/corrections/416", {"portions": {"nombre": 0}})[0], 400)
        self.assertEqual(self.json("PUT", "/api/corrections/416", {"portions": {"nombre": "6"}})[0], 400)

    def test_recherche_sans_accents(self):
        _, rep = self.json("GET", "/api/recherche?q=oeufs")
        self.assertEqual([r["id"] for r in rep["resultats"]], ["388"])
        _, rep = self.json("GET", "/api/recherche?q=POISSON")
        self.assertEqual([r["id"] for r in rep["resultats"]], ["416"])
        _, rep = self.json("GET", "/api/recherche?q=")
        self.assertEqual(rep["total"], 2)

    def test_page_web(self):
        code, entetes, brut = self.requete("GET", "/")
        self.assertEqual(code, 200)
        self.assertTrue(entetes["Content-Type"].startswith("text/html"))
        self.assertIn(b"Menu de la semaine", brut)

    def test_adresse_pour_la_3ds(self):
        from recettes3ds.serveur import adresse_pour_3ds
        self.assertEqual(adresse_pour_3ds("192.168.1.10:8383", 8383), "http://192.168.1.10:8383")
        self.assertEqual(adresse_pour_3ds("nas.local:8383", 8383), "http://nas.local:8383")
        # Page ouverte en localhost : on donne l'IP de la machine, jamais 127.0.0.1.
        locale = adresse_pour_3ds("127.0.0.1:8383", 8383)
        self.assertTrue(locale is None or not locale.startswith("http://127."))
        code, info = self.json("GET", "/api/info")
        self.assertIn("adresse_3ds", info)

    def test_zz_base_jamais_modifiee(self):
        # Passe en dernier (ordre alphabétique) : après toutes les requêtes,
        # aucun fichier de la base ne doit avoir changé.
        self.assertEqual(empreinte_dossier(self.source), self.empreinte)


if __name__ == "__main__":
    unittest.main()
