# Passerelle recettes → 3DS
# ----------------------------------------------------------------------------
# Cibles principales :
#   make venv     crée l'environnement Python local (.venv) avec Pillow
#   make test     lance les tests unitaires
#   make lancer   démarre le serveur en local sur la copie ../recettes
#   make rapport  analyse toute la base et affiche les cas douteux
#   make export-mealie  crée recettes-mealie.zip à importer dans Mealie
#   make export-sd      recettes pour la carte SD de la 3DS (IDS="416 388")
#   make docker   construit l'image Docker (à faire sur le NAS ou en local)
#   make clean    supprime l'environnement et les données de développement
# ----------------------------------------------------------------------------

PYTHON   ?= python3
VENV     := .venv
PY       := $(VENV)/bin/python
PORT     ?= 8383

# En local, on lit la copie de la base posée à côté du projet.
# Réglages propres à cette machine (non versionnés) : local.mk, par exemple
#   SOURCE_JSON = mon_export.json
#   ETIQUETTE   = Import
-include local.mk
SOURCE_DIR  ?= ../recettes
SOURCE_JSON ?= recettes.json
ETIQUETTE   ?=
DATA_DIR    ?= ./data-dev

.PHONY: venv test lancer rapport export-mealie export-sd docker clean

$(PY): requirements.txt
	$(PYTHON) -m venv $(VENV)
	$(PY) -m pip install --quiet --upgrade pip
	$(PY) -m pip install --quiet -r requirements.txt
	@touch $(PY)

venv: $(PY)

test: $(PY)
	$(PY) -m unittest discover -s tests -v

lancer: $(PY)
	RECETTES_SOURCE=$(SOURCE_DIR) RECETTES_JSON=$(SOURCE_JSON) RECETTES_DATA=$(DATA_DIR) RECETTES_PORT=$(PORT) \
		$(PY) -m recettes3ds

rapport: $(PY)
	$(PY) outils/rapport.py $(SOURCE_DIR) --json $(SOURCE_JSON)

# Archive au format Nextcloud Cookbook (schema.org) pour Mealie → /group/migrations.
export-mealie: $(PY)
	$(PY) outils/exporter_mealie.py $(SOURCE_DIR) recettes-mealie.zip --json $(SOURCE_JSON) \
		$(if $(ETIQUETTE),--etiquette "$(ETIQUETTE)")

# Recettes à copier sur la carte SD (dossier sd/3ds → racine de la carte).
IDS ?= 416 388 275
export-sd: $(PY)
	$(PY) outils/exporter_sd.py $(SOURCE_DIR) sd --json $(SOURCE_JSON) --ids $(IDS)

docker:
	docker build -t recettes3ds .

clean:
	rm -rf $(VENV) $(DATA_DIR)
	find . -name __pycache__ -type d -prune -exec rm -rf {} +
