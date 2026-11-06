# Étude : parcourir tout le catalogue depuis la 3DS

But : depuis la console, parcourir **toutes** les recettes (pas seulement le menu),
les **chercher**, les **filtrer** et gérer les **favoris**.

**Verdict : faisable**, sans obstacle technique bloquant. La mémoire, le réseau et
l'analyseur JSON de la console suffisent largement pour quelques centaines de
recettes. Le travail se répartit entre la passerelle (un nouvel index compact) et
l'appli (un nouvel écran). Une décision de conception reste à prendre : **où
vivent les favoris** (voir § 5).

---

## 1. Ce qui existe aujourd'hui

| Élément | État actuel | Conséquence pour le catalogue |
|---|---|---|
| Menu de la console | Favoris (cœur) Mealie, ou planning (`MENU_MEALIE`) | Le menu *est* déjà une liste de favoris : un « favori » ajouté depuis la 3DS entrerait au menu |
| Cache SD | `sdmc:/3ds/recettes/cache/<id>.json` + `<id>.r565` ; `nettoyer()` (synchro.c) supprime tout ce qui n'est pas au menu | Les recettes hors menu ne peuvent pas y rester telles quelles |
| `CATALOGUE_MAX` | 64 entrées (catalogue.h) | Trop petit pour ~400 recettes : il faut une structure à part |
| Tampon réseau | 64 Ko (`TAILLE_FICHIER_MAX`) | Trop petit pour un index complet (~60-100 Ko) |
| Analyseur JSON | Sans allocation, tableau de jetons fourni par l'appelant | Convient : il suffit d'un tableau de jetons plus grand, en statique |
| Liste à l'écran | 4 lignes de 46 px par page (doigts mouillés) | 400 recettes = 100 pages : il faut recherche, filtres et une liste plus dense |
| Passerelle → Mealie | GET uniquement (« ne modifie jamais Mealie ») | Gérer les favoris depuis la console oblige à revoir ce principe |
| Recherche passerelle | `/api/recherche` pour la source export JSON seulement ; 405 avec Mealie | À généraliser, ou à remplacer par un filtrage sur la console |
| Saisie de texte | `saisir_adresse()` (clavier logiciel swkbd) | Facile à généraliser en `saisir_texte()` pour la recherche |

## 2. Trois façons de faire

### A. Index complet téléchargé, filtrage sur la console (recommandé)

À la synchro, la console télécharge `GET /catalogue` : une ligne courte par recette
(id, titre, étiquettes, catégories, temps, favori, photo). Elle le garde sur la carte
SD et fait **toute la recherche et tous les filtres localement**. La recette complète
n'est téléchargée qu'à l'ouverture.

- ✅ Recherche instantanée, filtres combinables, fonctionne hors ligne (sauf l'ouverture
  d'une recette jamais téléchargée).
- ✅ Une seule requête par synchro ; la passerelle reste simple et sans état.
- ✅ Logique portable, testable sur le Mac avec clang comme le reste.
- ⚠️ Taille de l'index à surveiller au-delà de ~2 000 recettes (voir § 4).

### B. Requêtes à la demande (la passerelle relaie la recherche Mealie)

Chaque frappe de filtre ou de recherche déclenche `GET /catalogue?q=…&tag=…&page=…`.

- ✅ Aucune limite de taille côté console.
- ❌ Inutilisable hors ligne ; chaque changement de filtre bloque l'écran ~0,3-1 s
  (httpc est bloquant dans la boucle principale) ; la passerelle doit traduire
  chaque filtre pour chaque source (Mealie, export JSON).

### C. Tout télécharger sur la carte SD

- ❌ 400 recettes × ~4 Ko = 1,6 Mo de JSON, passe encore ; mais 400 photos × 192 Ko
  = **77 Mo** et plusieurs minutes de synchro. Disproportionné.

**Recommandation : A**, avec ouverture à la demande et photos à la demande (§ 6).

## 3. Nouvelle route de la passerelle : `GET /catalogue`

Format proposé (à valider avant d'écrire `docs/schema.md`) — les étiquettes et
catégories sont mises en dictionnaire pour ne pas répéter les textes 400 fois :

```json
{
  "v": 1,
  "rev": "a41f09c2",
  "etiquettes": ["Végétarien", "Congelable", "Rapide"],
  "categories": ["Entrée", "Plat", "Dessert"],
  "recettes": [
    { "id": "9f3c…", "titre": "Acras de poisson", "rev": "73638189",
      "e": [1], "c": [0], "min": 35, "fav": true, "img": true }
  ]
}
```

| Champ | Rôle |
|---|---|
| `rev` (racine) | Empreinte de tout l'index : `GET /catalogue?rev=a41f09c2` → `304` (ou `{"inchange": true}`) si rien n'a bougé |
| `e`, `c` | Indices dans `etiquettes` / `categories` (en mémoire : un masque de bits `uint64_t` par recette, ≤ 64 de chaque) |
| `min` | Temps total en minutes, ou `null` |
| `fav` | Favori du compte du jeton |
| `img` | La recette a une photo |

**Avec Mealie** : `GET /api/recipes` (liste paginée de résumés) contient déjà le nom,
les `tags`, `recipeCategory`, `tools`, `totalTime`, `image` et la date de
modification — **pas besoin de lire chaque recette** ; les favoris viennent de
`/api/users/self/favorites` (déjà utilisé). `totalTime` est un texte libre
(« 1 h 30 ») à convertir en minutes. *À vérifier dans le code Mealie v3.30 :
`perPage=-1` (tout d'un coup) et la forme exacte des résumés.*

**Avec l'export JSON** : tout est déjà en mémoire dans `SourceExportJson`.

Le `nb_etapes` / `nb_minuteurs` n'est pas dans les résumés Mealie : on l'omet de
l'index (il s'affiche une fois la recette ouverte).

## 4. Budget mémoire et réseau sur la console

Hypothèse : 400 recettes, prévoir jusqu'à 1 000.

| Poste | Estimation | Commentaire |
|---|---|---|
| Index JSON | ~150 o/recette → 60 Ko (400) / 150 Ko (1 000) | Tampon dédié de 256 Ko en statique |
| Jetons JSON | ~12 jetons/recette × 16-20 o → 100 Ko (400) / 240 Ko (1 000) | Tableau statique |
| Entrées en RAM | ~180 o (id 33, titre 96-128, masques, temps, drapeaux) → 72 Ko / 180 Ko | Statique, `CATALOGUE_DISTANT_MAX` 1 000 |
| Clés de tri / recherche | titre sans accents, ~100 o → 40 Ko / 100 Ko | Calculées une fois (`catalogue_cle_tri` existe) |
| **Total** | **~0,3 Mo (400) / ~0,7 Mo (1 000)** | La 3DS d'origine laisse ~64 Mo à l'appli : aucun souci |
| Téléchargement de l'index | 60 Ko en Wi-Fi 802.11g via httpc : ~0,2-0,5 s | Ordre de grandeur, à mesurer sur console |
| Ouverture d'une recette | 2-6 Ko : quasi instantané | |
| Photo pleine (400×240) | 192 Ko : ~0,5-1 s | Trop lent au défilement : vignette ou tâche de fond (§ 6) |

Recherche et filtres sur 1 000 entrées : quelques milliers de comparaisons de
chaînes courtes, bien en dessous d'une image (16 ms) même sur la 3DS d'origine.

## 5. Favoris : la décision à prendre

Le menu de la console **est** la liste des favoris Mealie. Mettre un cœur depuis la
3DS revient donc à ajouter la recette au menu, et elle sera téléchargée à la
synchro suivante — c'est cohérent et ça règle aussi la question du cache.

| Option | Principe | Pour | Contre |
|---|---|---|---|
| **F1. Écrire dans Mealie** (recommandé) | Nouvelles routes `PUT/DELETE /favoris/<id>` ; la passerelle appelle l'API Mealie des favoris (*`POST/DELETE /api/users/{id}/favorites/{slug}` à confirmer dans v3.30*) | Un seul jeu de favoris, partagé avec le téléphone et Mealie ; le menu suit tout seul | La passerelle cesse d'être « GET uniquement » ; la console doit envoyer PUT/DELETE (httpc le permet) ; impossible hors ligne |
| F2. Favoris locaux sur la SD | `sdmc:/3ds/recettes/favoris.txt` | Simple, hors ligne | Deux listes de favoris qui divergent ; le menu ne suit pas |
| F3. F1 + file d'attente | F1, et hors ligne on note l'action et on la rejoue à la synchro suivante | Le meilleur des deux | Un peu plus de code (conflits simples : la dernière action gagne) |

**Décision (2026-11-06) : F3.** Détail ci-dessous.

### Fonctionnement retenu (F3)

**Passerelle** — deux routes idempotentes (répéter la même action ne change rien) :

| Route | Effet | Réponse |
|---|---|---|
| `PUT /favoris/<id>` | met le cœur | `200 {"id": "…", "fav": true}` |
| `DELETE /favoris/<id>` | retire le cœur | `200 {"id": "…", "fav": false}` |

- Mealie : la passerelle lit une fois l'id de l'utilisateur du jeton (`GET /api/users/self`)
  et le `slug` de la recette (déjà connu par le cache des recettes), puis appelle l'API
  des favoris. Recette inconnue → `404` ; Mealie injoignable → `502` (comme aujourd'hui).
- Export JSON : `PUT` / `DELETE` reprennent `ajouter()` / `retirer()` du menu.
- Avec `MENU_MEALIE=planning`, le cœur ne change pas le menu (il reste utile comme filtre).

**Console** — un cœur touché :

1. change tout de suite l'index en mémoire et sur la carte (affichage immédiat) ;
2. si le Wi-Fi est là : `PUT`/`DELETE` ; si ça marche, c'est fini ;
3. sinon : l'action est notée dans `sdmc:/3ds/recettes/favoris_attente.txt`, une ligne
   par recette (`+<id>` ou `-<id>`). Une seule ligne par recette : la dernière action
   remplace la précédente, et deux actions qui s'annulent effacent la ligne ;
4. le cœur s'affiche « en attente » (contour seulement) tant qu'il n'est pas envoyé.

**Synchro** — la file est rejouée **avant** `GET /menu`, pour que le menu tienne
compte des cœurs mis hors ligne (et que les recettes ajoutées soient téléchargées
dans la foulée) :

- succès ou `404` (recette supprimée entre-temps) → la ligne est retirée ;
- passerelle injoignable ou `502` → on s'arrête, la file reste intacte ;
- conflit (cœur changé dans Mealie pendant que la console était hors ligne) :
  **l'action de la console gagne**. Simple et prévisible ; à revoir si ça gêne.
- L'écriture de la file est atomique (`fichier_ecrire_atomique`), comme le reste.

Le module de file (`favoris_attente.c`) est portable et testé sur le Mac avec un faux
téléchargeur, comme `synchro.c`.

Points de sécurité : la passerelle n'a pas d'authentification (réseau local). Avec
F1, n'importe qui sur le Wi-Fi pourrait changer les favoris du compte du jeton —
risque faible chez soi, mais à écrire dans le README. Le jeton Mealie a déjà les
droits nécessaires (ce sont ceux de l'utilisateur). Avec la source export JSON,
`PUT/DELETE /favoris` reprend simplement `ajouter()` / `retirer()` du menu.

## 6. Côté console

### Modules (dans le style actuel : logique portable + vue citro2d)

- **`index_catalogue.c/.h`** (portable, testé avec clang) : lecture de `/catalogue`,
  dictionnaires, masques de bits, recherche sans accents (tous les mots, dans le
  titre ; titre d'abord), filtres combinés, tri (titre, temps), lecture/écriture
  de `sdmc:/3ds/recettes/catalogue.json`.
- **`vue_catalogue.c/.h`** : nouvel écran, ouvert depuis la liste (touche dédiée ou
  bouton « Catalogue »).
- **`saisie.c`** : généraliser en `saisir_texte(indice, actuelle, …)`.
- **`reseau.c`** : ajouter `reseau_envoyer(methode, url, …)` pour PUT/DELETE (si F1).
- **`synchro.c`** : télécharger aussi l'index (avec `?rev=`) ; un index qui échoue
  ne bloque jamais la synchro du menu.

### Ouvrir une recette hors menu

Télécharger `GET /recipes/<id>` **en mémoire seulement**, sans l'écrire dans le
cache : `nettoyer()` et `CATALOGUE_MAX` restent inchangés. Si on la met en favori,
elle entre au menu et la synchro la range sur la carte. Hors ligne, les recettes
absentes du cache sont grisées (« Wi-Fi nécessaire »).

### Ergonomie (écran du bas 320×240, tactile)

On parcourt le catalogue *avant* de cuisiner, les mains propres : on peut se
permettre une liste plus dense que celle du menu.

```
┌──────────── écran du haut (400×240) ────────────┐
│  photo ou vignette de la recette sélectionnée   │
│  Acras de poisson          ♥  35 min            │
│  Entrée · Rapide · Congelable                   │
│  « poisson » · Entrée · ≤ 45 min — 12 / 412     │
└─────────────────────────────────────────────────┘
┌──────────── écran du bas (320×240) ─────────────┐
│ ♥ Acras de poisson                       35 min │  8 lignes
│   Agneau de sept heures                 7 h 10 │  de 24 px
│   Aïoli                                  15 min │
│ … │
│ [🔍 Chercher] [Filtres] [♥ seuls] [Retour]       │
└─────────────────────────────────────────────────┘
```

| Commande | Action |
|---|---|
| ↑ / ↓, toucher | choisir ; A ou toucher deux fois : ouvrir |
| L / R | lettre précédente / suivante (saut alphabétique) |
| Y | chercher (clavier logiciel) |
| X | panneau des filtres (catégories, étiquettes, temps max, avec photo) |
| SELECT | mettre / retirer le cœur |
| B | effacer les filtres, puis retour au menu |

Le panneau des filtres réutilise des « pastilles » tactiles : une page par famille
(catégories, étiquettes, temps), au plus ~12 pastilles par page.

### Photos

- Charger la photo de la sélection **après un temps d'arrêt** (le mécanisme
  `PHOTO_DELAI_MS` existe déjà dans vue_liste.c).
- Ajouter une **vignette** côté passerelle : `GET /recipes/<id>/vignette.rgb565`
  (ex. 160×96 = 30 Ko, 6× moins qu'une photo), même format `R565`.
- Télécharger dans un **fil d'exécution** (`threadCreate` de libctru) pour que
  le défilement reste fluide ; garder les ~16 dernières vignettes en mémoire
  (≈ 0,5 Mo).

## 7. Risques et points à vérifier

| Risque | Gravité | Parade |
|---|---|---|
| API Mealie v3.30 : forme des résumés, `perPage=-1`, route des favoris | Moyenne | Relire le code de Mealie v3.30 avant le lot 1 ; tests contre `FauxMealie` |
| `totalTime` Mealie en texte libre | Faible | Analyse « 1 h 30 / 90 min / PT1H30M », `null` sinon ; tests |
| Plus de 64 étiquettes ou catégories | Faible | Ne garder que les 64 plus fréquentes, ou masques sur 128 bits |
| httpc bloquant pendant l'ouverture d'une recette | Faible | 2-6 Ko : acceptable ; écran « Chargement… » |
| Fil de téléchargement des vignettes (concurrence avec la boucle principale) | Moyenne | File simple protégée par `LightLock`, un seul fil, annulation si la sélection change |
| Écriture dans Mealie (F1) | Moyenne | Routes limitées aux favoris, id validé (`_RE_ID`), tests ; documenter le risque réseau local |
| Index très gros (> 2 000 recettes) | Faible ici | Tronquer les titres, ou basculer vers l'option B |

## 8. Découpage proposé (une étape validée à la fois)

| Lot | Contenu | Où | Taille estimée |
|---|---|---|---|
| 1 | `GET /catalogue` pour les deux sources, `rev` global, tests ; mise à jour de `docs/schema.md` | passerelle | ~200 lignes + tests |
| 2 | `index_catalogue.c` : lecture, recherche, filtres, tri, cache SD ; tests clang | app3ds (portable) | ~350 lignes + tests |
| 3 | `vue_catalogue.c` : liste dense, recherche au clavier, ouverture à la demande | app3ds | ~400 lignes |
| 4 | Panneau des filtres (pastilles) | app3ds | ~200 lignes |
| 5 | Favoris (F3) : routes passerelle, `reseau_envoyer`, touche cœur, file d'attente rejouée à la synchro | les deux | ~350 lignes + tests |
| 6 | Vignettes : route passerelle, fil de téléchargement, cache mémoire | les deux | ~250 lignes |

Les lots 1 à 3 donnent déjà un catalogue utilisable (parcourir, chercher, ouvrir).

## 9. Questions pour décider

1. ~~**Favoris**~~ : **F3** retenu (Mealie + file d'attente hors ligne).
2. **Filtres utiles** : catégories, étiquettes, ustensiles, temps maximum, « avec photo »,
   « sans allergène X » ? (Les allergènes viennent des étiquettes « Allergène : … ».)
3. **Accès** : un bouton « Catalogue » sur l'écran du menu, ou le catalogue comme
   écran d'accueil avec un filtre « ♥ » activé par défaut ?
