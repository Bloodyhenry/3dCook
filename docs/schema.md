# Format JSON échangé entre la passerelle et la 3DS (version 1)

Toutes les réponses sont en UTF-8. Les clés sont en ASCII. Une recette fait
en général 2 Ko (5,6 Ko au maximum dans la base actuelle).

## `GET /menu`

```json
{
  "v": 1,
  "maj": 1762444800,
  "recettes": [
    { "id": "416", "titre": "Acras de poisson", "rev": "73638189", "image": true }
  ]
}
```

- `maj` : date de la dernière modification (menu, corrections ou base), en secondes epoch.
- `rev` : empreinte de la recette convertie. La 3DS compare cette valeur avec son cache SD
  et ne retélécharge que les recettes qui ont changé.
- `id` : texte de 1 à 32 caractères `[A-Za-z0-9_-]`. Avec Mealie, c'est l'UUID de la recette
  sans tirets. Ce texte peut servir tel quel de nom de fichier dans le cache SD.

Champs facultatifs, présents quand le menu vient d'un planning de repas (Mealie) :

| Champ | Exemple | Description |
|---|---|---|
| `date` | `"2026-11-07"` | jour prévu |
| `repas` | `"Dîner"` | Petit-déjeuner, Déjeuner, En-cas, Dîner, Accompagnement, Dessert, Boisson |
| `echelle` | `1.5` | multiplicateur choisi dans le planning, à appliquer par défaut sur la console |

Les entrées sont triées par date puis par repas. Une recette prévue plusieurs fois
n'apparaît qu'une fois, à sa première date.

Si la source ne répond pas, la passerelle renvoie le code HTTP 502 avec `{"erreur": "…"}`.
La 3DS utilise alors son cache.

## `GET /recipes/<id>`

```json
{
  "v": 1,
  "id": "416",
  "titre": "Acras de poisson",
  "rev": "73638189",
  "portions": null,
  "congelable": true,
  "image": true,
  "groupes": [
    { "titre": null,
      "items": [
        { "quantite": 220, "quantite_max": null, "unite": "g", "nom": "de poisson blanc", "allergene": true },
        { "quantite": 0.5, "quantite_max": null, "unite": null, "nom": "oignon", "allergene": false },
        { "quantite": null, "quantite_max": null, "unite": null, "nom": "persil", "allergene": false }
      ] }
  ],
  "etapes": [
    { "texte": "Faire cuire le poisson à la vapeur 15-20min." },
    { "texte": "Mélanger.", "section": "Pour la pâte" }
  ],
  "minuteurs": [
    { "etape": 0, "secondes": 900, "secondes_max": 1200, "libelle": "Vapeur 15-20 min" }
  ],
  "notes": ["Les acras se conservent 3 jours au réfrigérateur."],
  "conseil": "Les acras sont des petits beignets de poisson…"
}
```

| Champ | Description |
|---|---|
| `portions` | `null` ou `{ "nombre": 6, "unite": "ramequins" }`. Détecté automatiquement ou corrigé depuis la page web. |
| `groupes` | Au moins un groupe. `titre` vaut `null` ou un texte comme « Pour la pâte ». |
| `quantite`, `quantite_max` | Nombres, ou `null` quand la ligne n'a pas pu être analysée (jamais mise à l'échelle). `quantite_max` sert pour les plages (« 80-100 g »). |
| `unite` | `g`, `kg`, `mg`, `ml`, `cl`, `dl`, `l`, `c. à soupe`, `c. à café`, ou une unité comptable au singulier (`sachet`, `gousse`, `pincée`…). |
| `nom` | Reste de la ligne, « de »/« d' » compris. |
| `allergene` | La ligne contient un allergène (texte en rouge dans la base). |
| `etapes[].section` | *Facultatif.* Titre de sous-partie qui précède l'étape. |
| `minuteurs[].etape` | Indice (à partir de 0) dans `etapes`. La durée vient de l'étape dans Mealie quand elle y est saisie, sinon du texte de l'étape. |
| `secondes` / `secondes_max` | Durée de départ du minuteur (valeur basse d'une plage) et valeur haute (ou `null`). |
| `libelle` | Texte court (≈ 20 caractères) : « Four 180°C 30 min ». |

## `GET /recipes/<id>/image`

JPEG baseline (non progressif), qualité 80, au plus 400×240, environ 15 à 30 Ko.
La passerelle répond 404 si la recette n'a pas de photo (`"image": false`).

## Mise à l'échelle (à reproduire en C)

L'implémentation de référence est `passerelle/recettes3ds/echelle.py`. Ses tests
(`tests/test_echelle.py`) fixent le comportement attendu.

- Coefficient : si les portions sont connues, `voulu / portions.nombre`. Sinon,
  multiplicateur choisi (×½, ×1, ×1,5, ×2, ×3).
- Avec un coefficient de 1, les quantités sont affichées telles quelles.
- Arrondi au demi supérieur sur un pas qui dépend de l'unité :
  - g, ml : 0,5 sous 10 ; 1 sous 50 ; 5 sous 250 ; 10 au-delà
  - kg, l : 0,05 sous 1 ; 0,1 au-delà
  - cl, dl : 0,5 sous 10 ; 1 au-delà
  - autres unités et pièces : ¼ sous 10 ; 1 au-delà
  - un résultat positif n'est jamais arrondi à zéro
- Affichage : virgule décimale pour le métrique (« 1,5 l »), sinon fractions
  (« 1 ½ », « ¾ »). Pluriel des unités comptables à partir de 2 (« 2 gousses »).
