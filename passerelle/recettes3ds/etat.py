"""État propre à la passerelle : menu de la semaine et corrections.

Ces fichiers vivent dans le dossier de données de la passerelle
(/data dans Docker), jamais dans la base de recettes :
  menu.json         {"v": 1, "maj": <epoch>, "ids": ["416", "388"]}
  corrections.json  {"416": {"portions": {"nombre": 6, "unite": "ramequins"}}}
"""

import json
import os
import tempfile
import threading
import time
from pathlib import Path


def _ecrire_json(chemin: Path, donnees) -> None:
    """Écriture atomique : un plantage ne laisse jamais un fichier à moitié écrit."""
    chemin.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=chemin.parent, suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(donnees, f, ensure_ascii=False, indent=1)
        os.replace(tmp, chemin)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


def _lire_json(chemin: Path, defaut):
    try:
        with open(chemin, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return defaut


class Etat:
    def __init__(self, dossier: Path):
        self.dossier = Path(dossier)
        self._verrou = threading.Lock()
        self._chemin_menu = self.dossier / "menu.json"
        self._chemin_corr = self.dossier / "corrections.json"
        menu = _lire_json(self._chemin_menu, {})
        self._ids: list[str] = [str(i) for i in menu.get("ids", [])]
        self.maj: int = int(menu.get("maj", 0))
        self._corrections: dict = _lire_json(self._chemin_corr, {})

    # --- Menu ---------------------------------------------------------------

    def ids_menu(self) -> list[str]:
        with self._verrou:
            return list(self._ids)

    def _sauver_menu(self) -> None:
        self.maj = int(time.time())
        _ecrire_json(self._chemin_menu, {"v": 1, "maj": self.maj, "ids": self._ids})

    def ajouter(self, rid: str) -> bool:
        with self._verrou:
            if rid in self._ids:
                return False
            self._ids.append(rid)
            self._sauver_menu()
            return True

    def retirer(self, rid: str) -> bool:
        with self._verrou:
            if rid not in self._ids:
                return False
            self._ids.remove(rid)
            self._sauver_menu()
            return True

    # --- Corrections ----------------------------------------------------------

    def correction(self, rid: str) -> dict | None:
        with self._verrou:
            c = self._corrections.get(rid)
            return dict(c) if c else None

    def definir_portions(self, rid: str, portions: dict | None) -> None:
        """Fixe les portions d'une recette ; None rend la main à la détection."""
        with self._verrou:
            if portions is None:
                self._corrections.get(rid, {}).pop("portions", None)
                if not self._corrections.get(rid):
                    self._corrections.pop(rid, None)
            else:
                self._corrections.setdefault(rid, {})["portions"] = portions
            _ecrire_json(self._chemin_corr, self._corrections)
            self.maj = int(time.time())
