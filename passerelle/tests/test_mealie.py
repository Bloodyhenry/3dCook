"""Tests du connecteur Mealie, contre un faux serveur qui imite l'API v3.

Les réponses reprennent la forme de l'API Mealie v3.30 (champs en camelCase,
pagination items/page/total…) telle que décrite dans son code source.
"""

import datetime as dt
import io
import json
import shutil
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from recettes3ds.serveur import creer_serveur  # noqa: E402
from recettes3ds.sources.mealie import (  # noqa: E402
    SourceMealie, convertir_mealie, ingredient_mealie, portions_mealie, texte_markdown,
    unite_canonique,
)

ID_ACRAS = "6f1c2d3e-4a5b-4c6d-8e9f-0a1b2c3d4e5f"
ID_CAKE = "11111111-2222-4333-8444-555555555555"
JETON = "jeton-de-test"


def recette_acras(maj="2026-11-01T10:00:00Z"):
    """Recette telle que Mealie la renvoie après l'import de l'archive."""
    def libre(texte, titre=None):
        # Import d'une note seule : quantité 0, ni unité ni aliment.
        return {"quantity": 0, "unit": None, "food": None, "note": texte, "display": texte,
                "title": titre, "originalText": None, "referenceId": "x"}
    return {
        "id": ID_ACRAS, "slug": "acras-de-poisson", "name": "Acras de poisson",
        "image": "Zx9a", "recipeServings": 0, "recipeYieldQuantity": 15, "recipeYield": "acras",
        "description": "Les acras sont des **petits beignets**.",
        "tags": [{"name": "Import", "slug": "import"},
                 {"name": "Congelable", "slug": "congelable"},
                 {"name": "Allergène : Poisson", "slug": "allergene-poisson"},
                 {"name": "Allergène : Fruits À Coque", "slug": "allergene-fruits-a-coque"}],
        "recipeIngredient": [
            libre("220 g de poisson blanc", titre="Pâte"),
            libre("100ml de lait d'amande"),
            libre("persil"),
            {"quantity": 2, "unit": {"name": "cuillère à soupe", "abbreviation": "c. à s.",
                                     "useAbbreviation": True},
             "food": {"name": "huile d'olive", "pluralName": None}, "note": "",
             "display": "2 c. à s. huile d'olive", "title": "Cuisson"},
        ],
        "recipeInstructions": [
            {"id": "a", "title": "", "summary": "", "text": "Faire cuire le poisson à la vapeur 15-20min.",
             "durationSeconds": None},
            {"id": "b", "title": "", "summary": "", "text": "Mixer le tout.", "durationSeconds": None},
            {"id": "c", "title": "Cuisson", "summary": "",
             "text": "Faire cuire à 200°C pendant 15-20min.", "durationSeconds": 1080},
        ],
        "notes": [{"title": "", "text": "Se conservent 3 jours au réfrigérateur."},
                  {"title": "Astuce", "text": "Retourner à mi-cuisson."}],
        "updatedAt": maj,
    }


def recette_cake():
    return {
        "id": ID_CAKE, "slug": "cake", "name": "Cake", "image": None,
        "recipeServings": 4, "recipeYieldQuantity": 0, "recipeYield": "",
        "description": "", "tags": [], "notes": [],
        "recipeIngredient": [{"quantity": 0, "unit": None, "food": None,
                              "note": "2 œufs", "display": "2 œufs"}],
        "recipeInstructions": [{"text": "Cuire 30 min.", "title": None}],
        "updatedAt": "2026-11-02T08:00:00Z",
    }


class FauxMealie(BaseHTTPRequestHandler):
    """Imite les routes de Mealie utilisées par la passerelle."""
    recettes: dict = {}
    planning: list = []
    favoris: list = []
    image_webp: bytes = b""
    appels: list = []

    def log_message(self, *args):
        pass

    def _json(self, code, donnees):
        corps = json.dumps(donnees).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(corps)))
        self.end_headers()
        self.wfile.write(corps)

    def do_GET(self):
        url = urlparse(self.path)
        FauxMealie.appels.append((url.path, parse_qs(url.query)))
        if self.headers.get("Authorization") != f"Bearer {JETON}":
            return self._json(401, {"detail": "Not authenticated"})
        if url.path == "/api/users/self/favorites":
            return self._json(200, {"ratings": self.favoris})
        if url.path == "/api/households/mealplans":
            return self._json(200, {"page": 1, "per_page": 200, "total": len(self.planning),
                                    "total_pages": 1, "items": self.planning})
        if url.path.startswith("/api/recipes/"):
            r = self.recettes.get(url.path.rsplit("/", 1)[1])
            return self._json(200, r) if r else self._json(404, {"detail": "Not found"})
        if url.path == f"/api/media/recipes/{ID_ACRAS}/images/original.webp":
            self.send_response(200)
            self.send_header("Content-Type", "image/webp")
            self.send_header("Content-Length", str(len(self.image_webp)))
            self.end_headers()
            self.wfile.write(self.image_webp)
            return None
        return self._json(404, {"detail": "Not found"})


def resume(r):
    return {"id": r["id"], "name": r["name"], "slug": r["slug"], "updatedAt": r["updatedAt"]}


class TestConversionMealie(unittest.TestCase):
    def test_recette_importee(self):
        r = convertir_mealie(recette_acras(), "abc", image=True)
        self.assertEqual(r["titre"], "Acras de poisson")
        self.assertEqual(r["portions"], {"nombre": 15, "unite": "acras"})
        self.assertTrue(r["congelable"])
        self.assertEqual(r["conseil"], "Les acras sont des petits beignets.")
        self.assertEqual([g["titre"] for g in r["groupes"]], ["Pâte", "Cuisson"])
        pate = r["groupes"][0]["items"]
        # Texte libre → analyse française, allergènes depuis les étiquettes.
        self.assertEqual(pate[0], {"quantite": 220, "quantite_max": None, "unite": "g",
                                   "nom": "de poisson blanc", "allergene": True})
        self.assertTrue(pate[1]["allergene"])          # amande → fruits à coque
        self.assertFalse(pate[2]["allergene"])         # persil
        self.assertEqual(r["notes"], ["Se conservent 3 jours au réfrigérateur.",
                                      "Astuce : Retourner à mi-cuisson."])

    def test_ingredient_structure(self):
        r = convertir_mealie(recette_acras(), "abc", image=True)
        huile = r["groupes"][1]["items"][0]
        self.assertEqual(huile, {"quantite": 2, "quantite_max": None, "unite": "c. à soupe",
                                 "nom": "huile d'olive", "allergene": False})

    def test_minuteurs(self):
        r = convertir_mealie(recette_acras(), "abc", image=True)
        # Étape 0 : durée lue dans le texte ; étape 2 : durée saisie dans Mealie.
        self.assertEqual([(m["etape"], m["secondes"], m["secondes_max"]) for m in r["minuteurs"]],
                         [(0, 900, 1200), (2, 1080, None)])
        self.assertEqual(r["minuteurs"][1]["libelle"], "Four 200°C 18 min")
        self.assertEqual(r["etapes"][2], {"texte": "Faire cuire à 200°C pendant 15-20min.",
                                          "section": "Cuisson"})

    def test_portions(self):
        self.assertEqual(portions_mealie({"recipeServings": 4}), {"nombre": 4, "unite": "portions"})
        self.assertEqual(portions_mealie({"recipeYield": "6 ramequins"}),
                         {"nombre": 6, "unite": "ramequins"})
        self.assertIsNone(portions_mealie({}))

    def test_unites(self):
        self.assertEqual(unite_canonique({"name": "Gramme", "abbreviation": "g"}), "g")
        self.assertEqual(unite_canonique({"name": "cuillère à café", "abbreviation": ""}), "c. à café")
        self.assertEqual(unite_canonique({"name": "botte de radis", "abbreviation": "bt",
                                          "useAbbreviation": True}), "bt")
        self.assertIsNone(unite_canonique(None))

    def test_markdown(self):
        self.assertEqual(texte_markdown("**Préchauffer** le [four](http://x)\n\n- à 180°C"),
                         "Préchauffer le four à 180°C")

    def test_ingredient_vide(self):
        self.assertIsNone(ingredient_mealie({"quantity": 0, "note": "", "display": ""}, set()))


class TestSourceMealie(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp())
        tampon = io.BytesIO()
        Image.new("RGB", (1000, 750), (90, 140, 60)).save(tampon, "WEBP")
        FauxMealie.image_webp = tampon.getvalue()
        FauxMealie.recettes = {ID_ACRAS: recette_acras(), ID_CAKE: recette_cake()}
        FauxMealie.planning = [
            {"id": 2, "date": "2026-11-07", "entryType": "dinner", "recipeId": ID_CAKE,
             "recipe": resume(recette_cake()), "recipeScale": 1.5, "updatedAt": "2026-11-03T09:00:00Z"},
            {"id": 1, "date": "2026-11-07", "entryType": "lunch", "recipeId": ID_ACRAS,
             "recipe": resume(recette_acras()), "recipeScale": 1, "updatedAt": "2026-11-03T08:00:00Z"},
            {"id": 3, "date": "2026-11-08", "entryType": "lunch", "recipeId": ID_ACRAS,
             "recipe": resume(recette_acras()), "recipeScale": 1},
            {"id": 4, "date": "2026-11-09", "entryType": "dinner", "recipeId": None,
             "recipe": None, "title": "Restaurant"},
        ]
        cls.mealie = ThreadingHTTPServer(("127.0.0.1", 0), FauxMealie)
        threading.Thread(target=cls.mealie.serve_forever, daemon=True).start()
        url_mealie = f"http://127.0.0.1:{cls.mealie.server_address[1]}"

        cls.source = SourceMealie(url_mealie, JETON, cls.tmp / "data", jours=7,
                                  lien="http://nas:9925", aujourd_hui=lambda: dt.date(2026, 11, 6),
                                  menu="planning")
        cls.serveur = creer_serveur(cls.source, "127.0.0.1", 0)
        cls.url = f"http://127.0.0.1:{cls.serveur.server_address[1]}"
        threading.Thread(target=cls.serveur.serve_forever, daemon=True).start()
        cls.url_mealie = url_mealie

    @classmethod
    def tearDownClass(cls):
        for s in (cls.serveur, cls.mealie):
            s.shutdown()
            s.server_close()
        shutil.rmtree(cls.tmp)

    def requete(self, methode, chemin):
        req = urllib.request.Request(self.url + chemin, method=methode)
        try:
            with urllib.request.urlopen(req, timeout=5) as rep:
                return rep.status, rep.read()
        except urllib.error.HTTPError as e:
            return e.code, e.read()

    def test_menu_depuis_le_planning(self):
        FauxMealie.appels.clear()
        code, brut = self.requete("GET", "/menu")
        self.assertEqual(code, 200)
        menu = json.loads(brut)
        acras, cake = ID_ACRAS.replace("-", ""), ID_CAKE.replace("-", "")
        # Tri par date puis par repas, sans doublon, sans les entrées sans recette.
        self.assertEqual([(r["id"], r["repas"]) for r in menu["recettes"]],
                         [(acras, "Déjeuner"), (cake, "Dîner")])
        self.assertEqual(menu["recettes"][1]["echelle"], 1.5)
        self.assertEqual(menu["recettes"][0]["date"], "2026-11-07")
        self.assertGreater(menu["maj"], 0)
        # Paramètres envoyés à Mealie.
        chemin, params = [a for a in FauxMealie.appels if a[0] == "/api/households/mealplans"][0]
        self.assertEqual(params["start_date"], ["2026-11-06"])
        self.assertEqual(params["end_date"], ["2026-11-12"])
        self.assertEqual(params["include_week_entries"], ["true"])

    def test_cache_selon_updated_at(self):
        self.requete("GET", "/menu")
        FauxMealie.appels.clear()
        self.requete("GET", "/menu")
        # Recettes inchangées : seul le planning est relu.
        self.assertEqual([a[0] for a in FauxMealie.appels], ["/api/households/mealplans"])

    def test_recette_et_image(self):
        rid = ID_ACRAS.replace("-", "")
        code, brut = self.requete("GET", f"/recipes/{rid}")
        self.assertEqual(code, 200)
        self.assertEqual(json.loads(brut)["id"], rid)
        code, brut = self.requete("GET", f"/recipes/{rid}/image")
        self.assertEqual(code, 200)
        with Image.open(io.BytesIO(brut)) as img:
            self.assertEqual((img.format, img.size), ("JPEG", (320, 240)))
        code, _ = self.requete("GET", f"/recipes/{ID_CAKE.replace('-', '')}/image")
        self.assertEqual(code, 404)

    def test_identifiants_invalides(self):
        self.assertEqual(self.requete("GET", "/recipes/pas-un-uuid")[0], 404)
        self.assertEqual(self.requete("GET", "/recipes/" + "0" * 32)[0], 404)

    def test_ecritures_refusees(self):
        rid = ID_ACRAS.replace("-", "")
        self.assertEqual(self.requete("PUT", f"/api/menu/{rid}")[0], 405)
        self.assertEqual(self.requete("DELETE", f"/api/menu/{rid}")[0], 405)
        self.assertEqual(self.requete("PUT", f"/api/corrections/{rid}")[0], 405)
        self.assertEqual(self.requete("GET", "/api/recherche?q=x")[0], 405)

    def test_info(self):
        code, brut = self.requete("GET", "/api/info")
        info = json.loads(brut)
        info.pop("adresse_3ds")   # dépend de la machine de test
        self.assertRegex(info.pop("code"), r"^[0-9a-f]{10}$")
        self.assertEqual(info, {"source": "Mealie", "lien": "http://nas:9925",
                                            "menu_modifiable": False, "corrections": False,
                                            "recherche": False, "menu": "planning"})

    def test_menu_depuis_les_favoris(self):
        FauxMealie.favoris = [
            {"recipeId": ID_ACRAS, "rating": None, "isFavorite": True},
            {"recipeId": ID_CAKE, "rating": 4, "isFavorite": True},
            {"recipeId": ID_CAKE, "rating": 4, "isFavorite": True},          # doublon
            {"recipeId": "99999999-0000-4000-8000-000000000000", "isFavorite": True},  # disparue
        ]
        favoris = SourceMealie(self.url_mealie, JETON, self.tmp / "d4")      # « favoris » par défaut
        self.assertEqual(favoris.info()["menu"], "favoris")
        menu = favoris.menu()
        # Triés par titre sans accents (« Acras », puis « Cake »), sans doublon ni recette disparue,
        # et sans jour ni repas (ce ne sont pas des repas planifiés).
        self.assertEqual([r["titre"] for r in menu["recettes"]], ["Acras de poisson", "Cake"])
        self.assertEqual(set(menu["recettes"][0]), {"id", "titre", "rev", "image"})
        self.assertEqual(menu["recettes"][0]["id"], ID_ACRAS.replace("-", ""))
        self.assertGreater(menu["maj"], 0)
        FauxMealie.favoris = []
        self.assertEqual(favoris.menu()["recettes"], [])

    def test_menu_mealie_invalide(self):
        with self.assertRaises(ValueError):
            SourceMealie(self.url_mealie, JETON, self.tmp / "d5", menu="semaine")

    def test_mealie_injoignable_ou_jeton_refuse(self):
        eteint = SourceMealie("http://127.0.0.1:9", JETON, self.tmp / "d2")
        serveur = creer_serveur(eteint, "127.0.0.1", 0)
        threading.Thread(target=serveur.serve_forever, daemon=True).start()
        try:
            with self.assertRaises(urllib.error.HTTPError) as e:
                urllib.request.urlopen(f"http://127.0.0.1:{serveur.server_address[1]}/menu", timeout=5)
            self.assertEqual(e.exception.code, 502)
        finally:
            serveur.shutdown()
            serveur.server_close()

        mauvais = SourceMealie(self.url_mealie, "faux", self.tmp / "d3")
        from recettes3ds.sources import SourceIndisponible
        with self.assertRaisesRegex(SourceIndisponible, "jeton"):
            mauvais.menu()


if __name__ == "__main__":
    unittest.main()
