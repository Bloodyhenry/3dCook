"""Sources de recettes : chaque connecteur fournit les mêmes services au serveur.

  - export_json : un export statique de recettes (lecture seule), menu géré
    par la page web de la passerelle ;
  - mealie : un serveur Mealie, menu tiré de son planning de repas.

Pour ajouter un gestionnaire (Tandoor, Nextcloud Cookbook…), il suffit
d'écrire une classe qui hérite de Source.
"""

from pathlib import Path


class SourceIndisponible(Exception):
    """La source ne répond pas (NAS éteint, Mealie arrêté, jeton refusé…)."""


class Source:
    nom = "?"
    lien: str | None = None          # adresse à ouvrir pour gérer les recettes
    menu_modifiable = False          # la page web peut-elle changer le menu ?
    corrections = False              # la page web peut-elle corriger les portions ?
    recherche = False                # la page web peut-elle chercher dans la base ?

    def recette(self, rid: str) -> dict | None:
        """Recette au format 3DS, ou None si elle n'existe pas."""
        raise NotImplementedError

    def chemin_image(self, rid: str) -> Path | None:
        """Photo JPEG ≤ 400×240 prête à servir, ou None."""
        raise NotImplementedError

    def menu(self) -> dict:
        """Contenu de GET /menu."""
        raise NotImplementedError

    def info(self) -> dict:
        return {
            "source": self.nom,
            "lien": self.lien,
            "menu_modifiable": self.menu_modifiable,
            "corrections": self.corrections,
            "recherche": self.recherche,
        }
