# Passerelle recettes → 3DS

Ce petit serveur HTTP tourne sur le NAS. Il lit les recettes **sans jamais les modifier**
et les sert à la 3DS dans un JSON léger (format décrit dans
[../docs/schema.md](../docs/schema.md)).

La passerelle sait lire deux sources :

| Source | Recettes | Menu de la 3DS |
|---|---|---|
| **Mealie** (recommandée) | API REST de Mealie | recettes favorites (cœur) du compte du jeton, ou planning de repas |
| Export JSON | export statique d'un site de recettes, en lecture seule | page web de la passerelle |

Pour ajouter un autre gestionnaire (Tandoor, Nextcloud Cookbook…), il suffit
d'écrire une classe dans `recettes3ds/sources/`.

## Structure

```
passerelle/
├── Makefile                   venv, test, lancer, rapport, export-mealie, docker
├── Dockerfile
├── docker-compose.yml         Mealie + passerelle pour le Synology
├── .env.exemple               réglages à copier en .env
├── recettes3ds/
│   ├── sources/
│   │   ├── mealie.py          connecteur Mealie (API v3, lecture seule)
│   │   └── export_json.py     export statique + conversion vers schema.org
│   ├── construire.py          assemblage du JSON 3DS (minuteurs, rev)
│   ├── texte.py               HTML → lignes propres, mots en rouge
│   ├── ingredients.py         quantité / unité / nom, groupes, portions
│   ├── etapes.py              découpage en étapes, sections et notes
│   ├── minuteurs.py           durées et libellés des minuteurs
│   ├── allergenes.py          14 allergènes UE, étiquettes « Allergène : … »
│   ├── echelle.py             mise à l'échelle (référence pour le code C)
│   ├── images.py              photo réduite à 400×240 max
│   ├── etat.py                menu.json, corrections.json (source export JSON)
│   ├── serveur.py             endpoints HTTP
│   └── web/index.html         page du menu (téléphone)
├── outils/
│   ├── exporter_mealie.py     export JSON → archive à importer dans Mealie
│   └── rapport.py             statistiques d'analyse sur l'export
└── tests/
```

## Développement sur le Mac

```bash
make test             # tests (dont un faux serveur Mealie)
make export-mealie    # crée recettes-mealie.zip depuis ../recettes
make lancer           # passerelle sur l'export JSON : http://127.0.0.1:8383/
```

Par défaut, l'export JSON est lu dans `../recettes/recettes.json`. Pour une autre
base, créer un fichier `local.mk` (non versionné) à côté du Makefile :

```make
SOURCE_DIR  = ../ma-base
SOURCE_JSON = mon_export.json
ETIQUETTE   = Import      # étiquette Mealie ajoutée à chaque recette exportée
```

## Installation sur le Synology

1. **Déposer le projet.** Copier le *contenu* du dossier `passerelle/` (Dockerfile,
   docker-compose.yml, recettes3ds/…) directement dans `/volume1/docker/recettes3ds/`,
   sans sous-dossier.
   Copier `.env.exemple` en `.env` et y mettre l'adresse IP du NAS (`NAS_ADRESSE`).
2. **Démarrer.** Dans Container Manager, aller dans **Projet**, puis **Créer**, et choisir ce dossier.
   Mealie répond ensuite sur `http://<NAS>:9925`.
3. **Première connexion à Mealie.** Utiliser `changeme@example.com` / `MyPassword`,
   puis changer tout de suite l'adresse et le mot de passe dans le profil.
4. **Importer les recettes.** Aller sur la page `/group/migrations`, choisir
   **Nextcloud Cookbook**, puis envoyer `recettes-mealie.zip`.
   Les allergènes deviennent des étiquettes « Allergène : … ».
5. **Créer un jeton d'API.** Aller sur la page `/user/profile/api-tokens`, créer
   un jeton nommé `3DS`, le copier dans `.env` (`MEALIE_JETON=…`), puis relancer le projet.
6. **Choisir les recettes de la console** : dans Mealie, chercher une recette et toucher son
   **cœur** (favori), avec le compte qui a créé le jeton. Toutes les recettes favorites forment le
   menu de la 3DS, triées par titre (`http://<NAS>:8383/` pour vérifier). Retirer le cœur la
   retire de la console à la synchronisation suivante.
   Pour utiliser plutôt le planning de repas des 7 prochains jours, mettre
   `MENU_MEALIE=planning` dans `.env`.

## Mettre à jour la passerelle du NAS

Depuis le Mac, avec le partage `docker` du NAS monté dans le Finder :

```bash
make deployer-nas
```

La commande lance les tests, puis copie le code dans `docker/recettes3ds/passerelle`. Elle ne
touche ni à `.env`, ni à `data/`, ni aux archives, et retire les anciens fichiers de code qui
n'existent plus. Ensuite, dans Container Manager, ouvrir le projet `recettes3ds`, puis faire
**Arrêter** et **Construire**. Enfin :

```bash
make verifier-nas
```

Cette commande compare l'empreinte du code du Mac à celle que la passerelle du NAS indique dans
`/api/info`. Elle répond « À jour », ou « Ancien code » si le NAS tourne encore sur l'ancienne
image.

**Une fois pour toutes**, ajouter `pull_policy: build` sous `image: recettes3ds:latest` dans le
`compose.yaml` du NAS (le `docker-compose.yml` d'ici l'a déjà). Sinon, Container Manager réutilise
l'ancienne image et ignore le nouveau code. À défaut, supprimer l'image `recettes3ds` (onglet
**Image**) avant **Construire**.

L'emplacement du projet et l'adresse du NAS se changent dans `local.mk` (`NAS_PROJET`, `NAS_URL`).

Pour mieux exploiter les données, tu peux compléter dans Mealie :

- **Portions** : `recipeYield`, par exemple « 6 ramequins ». Un export n'en indique pas toujours.
- **Durée des étapes** : elle prime sur les durées que la passerelle lit dans le texte.

La passerelle et Mealie n'ont pas d'accès public : n'ouvre pas les ports
8383 et 9925 vers Internet. Si tes recettes viennent d'un site sous abonnement,
garde ton instance Mealie privée.
