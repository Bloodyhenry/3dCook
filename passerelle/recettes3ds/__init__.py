"""Passerelle entre la base de recettes du NAS et l'appli 3DS.

La base d'origine est lue en lecture seule. Tout ce que la passerelle écrit
(menu de la semaine, corrections, cache d'images) va dans un dossier à part.
"""

VERSION_SCHEMA = 1
