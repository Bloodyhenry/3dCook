# Appli 3DS « Recettes mains sales »

Programme homebrew pour Nintendo 3DS, écrit en C avec **libctru**, la bibliothèque système des
homebrews 3DS, et **citro2d**, qui sert à dessiner en 2D. On pose la console sur le plan de travail
et on suit une recette étape par étape, sans la toucher quand on a les mains sales.

L'appli ne parle jamais directement à Mealie. Elle interroge seulement la **passerelle** qui tourne
sur le NAS (dossier [../passerelle](../passerelle)). Le format des données échangées est décrit
dans [../docs/schema.md](../docs/schema.md).

---

## 1. Où en est le projet

Le projet avance en six étapes. On valide chaque étape sur la console avant de passer à la suivante.

| Étape | Ce qu'elle apporte | État |
|---|---|---|
| 1 | Afficher une recette écrite directement dans le code (« en dur ») | **validée sur la console** (6 novembre 2026) |
| 2 | Lire les recettes au format JSON sur la carte SD, et choisir dans une liste | **validée sur la console** (6 novembre 2026) |
| 3 | Récupérer le menu et les recettes depuis la passerelle par Wi-Fi, et les garder sur la SD pour cuisiner sans Wi-Fi | **validée sur la console** (6 novembre 2026) |
| 4 | Minuteurs en parallèle, alarme sonore, LED qui clignote, mise en veille bloquée pendant la cuisson | **validée sur la console** (6 novembre 2026) |
| 5 | Souffler dans le micro pour passer à l'étape suivante | **validée sur la console** (6 novembre 2026) |
| 6 | Passer la main devant la caméra intérieure pour passer à l'étape suivante | à faire |

### Ce que fait l'étape 1

Elle affiche une seule recette, « Acras de poisson », écrite dans `source/recette.c`.

- **Écran du haut** (400 × 240 pixels) :
  - en haut à gauche le titre de la recette, en haut à droite le numéro d'étape (« Étape 3 / 6 ») ;
  - le nom de la section en orange quand l'étape en a une (« Cuisson ») ;
  - le texte de l'étape **en gros caractères**. L'appli essaie plusieurs tailles et garde la plus
    grande où le texte tient en entier : une étape courte s'affiche très gros, une étape longue
    plus petit ;
  - tout en bas, une barre orange qui montre l'avancement dans la recette.
- **Écran du bas** (320 × 240 pixels, tactile) :
  - en haut, les portions avec un bouton **-** et un bouton **+**. Si la recette indique ses
    portions (« 15 acras »), on règle ce nombre. Sinon, on choisit un multiplicateur parmi
    ×½, ×1, ×1 ½, ×2, ×3 et ×4 ;
  - au milieu, la liste des ingrédients. Les quantités sont **recalculées** à chaque changement
    de portions, par exemple « 220 g » devient « 440 g » quand on double. Les titres de groupe
    (« Pour la cuisson ») sont en orange et les ingrédients allergènes en orange clair. Deux petits
    triangles gris signalent qu'il y a d'autres lignes au-dessus ou en dessous ;
  - en bas, deux gros boutons **Précédent** et **Suivant**, assez larges pour un doigt.

La recette d'exemple contient exprès une étape longue, une section et deux groupes
d'ingrédients : ça permet de tester la mise en page.

### Ce que font les étapes 2 et 3

Plus besoin de sortir la carte SD : **l'appli télécharge elle-même le menu par Wi-Fi.**

- **Au démarrage**, l'appli se connecte à la passerelle et télécharge le menu de la semaine. Elle
  ne télécharge que les recettes qui ont changé depuis la dernière fois, grâce à leur empreinte
  `rev`. Elle les range sur la carte SD, dans `3ds/recettes/cache/<id>.json`, et garde le menu
  dans `3ds/recettes/menu.json`.
- **Sans Wi-Fi, ou si la passerelle ne répond pas**, l'appli le dit en orange et affiche les
  recettes déjà présentes sur la carte. On peut donc cuisiner hors ligne.
- **Le menu** affiche les recettes dans l'ordre du planning, avec le jour et le repas quand la
  source les donne (Mealie) : « Samedi 7 · Dîner · 6 étapes ». Si le planning prévoit ×1,5, la
  recette s'ouvre directement à ×1,5.
- **Écran du bas** : une ligne par recette, et **toucher une ligne ouvre la recette**. Quatre
  boutons en dessous :
  - **Haut** et **Bas** changent de page ;
  - **Synchro** télécharge de nouveau le menu ;
  - **Adresse** ouvre le clavier de la console pour saisir l'adresse de la passerelle.
- **Dans une recette**, le bouton **Liste** (en haut à gauche de l'écran du bas) ou **SELECT**
  ramène à la liste.
- **Une recette retirée du menu** est aussi retirée de la carte à la synchronisation suivante.
  Le cache reflète toujours le menu.

**Aucun risque pour la carte SD.** L'appli écrit uniquement dans son dossier `3ds/recettes/`, et
toujours de façon « atomique » : elle écrit d'abord un fichier temporaire, puis le renomme. Une
coupure en pleine écriture laisse l'ancien fichier intact. De plus, une recette qui arrive
incomplète ou abîmée est vérifiée et refusée avant de remplacer quoi que ce soit.

Les recettes sont lues par un petit analyseur JSON écrit pour l'appli (`json.c`), sans
bibliothèque externe. devkitPro ne propose pas cJSON, et les bibliothèques disponibles (jansson,
json-c) allouent de la mémoire pour chaque valeur. L'analyseur a été vérifié sur les 400 recettes
de la base : toutes sont lues à l'identique de ce qu'écrit la passerelle.

### Ce que fait l'étape 4

Les **minuteurs** viennent des durées repérées par la passerelle dans le texte des étapes
(« Four 180°C 30 min »), ou saisies sur l'étape dans Mealie.

- **Lancer** : quand l'étape affichée a un minuteur, un bouton apparaît sur l'écran du bas,
  au-dessus de **Précédent** et **Suivant**. Un toucher le lance. Pour une plage (« 15-20 min »),
  le minuteur part sur la valeur basse, pour vérifier la cuisson à temps.
- **Plusieurs en parallèle** (jusqu'à 6) : ils continuent quand on change d'étape, qu'on revient
  à la liste ou qu'on ouvre une autre recette.
- **Bandeau en bas de l'écran du haut** : les deux minuteurs les plus proches de la fin, avec leur
  temps restant en gros, lisible de loin. S'il y en a plus de deux, « +1 », « +2 » s'affiche à droite.
- **Arrêter un minuteur qui tourne** : toucher son bouton, qui devient orange (« Arrêter ce
  minuteur ? »), puis toucher encore dans les 3 secondes. Le double toucher évite d'en annuler un
  par erreur.
- **À la fin** :
  - un double bip répété, généré par l'appli ;
  - la LED de notification qui clignote en orange ;
  - le bandeau du haut qui clignote, et l'écran du bas qui affiche **« C'est prêt ! »**.

  **N'importe quel toucher ou bouton** (sauf START) arrête l'alarme, sans autre effet.
- **Pas de mise en veille** tant qu'un minuteur tourne : on peut refermer la console, le
  décompte et l'alarme continuent.
- **Quitter avec START** alors que des minuteurs tournent demande d'appuyer deux fois, car les
  minuteurs sont perdus en quittant.
- **Pour essayer sans attendre** : **SELECT** dans la liste ouvre la recette d'exemple. Son
  étape 2 a un minuteur de test de 10 secondes.

**Le son** passe par le DSP de la console. Il a besoin du fichier `sdmc:/3ds/dspfirm.cdc`, que
l'outil homebrew **DSP1** extrait une fois pour toutes (disponible dans Universal-Updater). Sans
ce fichier, l'appli essaie un second système de son, csnd. Si aucun ne marche, l'alarme reste
visuelle : LED et écran. Le volume suit le curseur de volume de la console.

### Ce que fait l'étape 5

**Souffler vers le micro passe à l'étape suivante.** Le micro se trouve sous l'écran du bas, à
gauche des boutons. Pas besoin de toucher la console.

- **Ce qui déclenche** : un son fort et **continu** pendant au moins 0,25 seconde, comme un
  souffle. Un bruit bref (casserole, claquement) ou la voix, faite de syllabes séparées par de
  petits silences, ne déclenchent pas.
- **Pas de double passage** : après un souffle reconnu, le micro est ignoré 1,5 seconde, puis il
  faut un retour au calme. Un long souffle ne fait donc avancer que d'une étape.
- **Pendant une alarme**, et 1 seconde après, le micro est ignoré : le bip de l'alarme ne fait
  pas avancer la recette.
- **Indicateur** : dans une recette, une petite jauge en haut de l'écran du haut, à gauche du
  numéro d'étape, montre ce qu'entend le micro. Un trait blanc marque le seuil, et la jauge
  devient orange quand un souffle est reconnu.
- **Écran Réglages** (bouton **Réglages**, ou **X**, dans la liste) :
  - l'adresse de la passerelle ;
  - le micro activé ou coupé (**A**) ;
  - la **sensibilité** de 1 à 5 (boutons **-** / **+**, ou croix gauche / droite) ;
  - une **grande jauge en direct**. On souffle, on regarde si la barre dépasse le trait, et
    « Souffle détecté ! » s'affiche quand c'est reconnu.

  Les réglages sont enregistrés dans `3ds/recettes/config.ini`.

---

## 2. Commandes sur la console

| Action | Boutons physiques | Écran tactile |
|---|---|---|
| Étape suivante | **A**, **R** ou flèche **→** de la croix | bouton **Suivant** |
| Étape précédente | **B**, **L** ou flèche **←** de la croix | bouton **Précédent** |
| Faire défiler les ingrédients | flèches **↑** / **↓** (croix ou stick) | toucher la moitié haute ou basse de la liste |
| Plus de portions | **X** | bouton **+** |
| Moins de portions | **Y** | bouton **-** |
| Étape suivante, sans les mains | souffler vers le micro (sous l'écran du bas) | |
| Lancer le minuteur de l'étape | | toucher son bouton (au-dessus de Précédent / Suivant) |
| Arrêter un minuteur qui tourne | | toucher son bouton, puis encore dans les 3 s |
| Arrêter l'alarme | n'importe quel bouton sauf START | toucher l'écran |
| Revenir à la liste des recettes | **SELECT** | bouton **Liste** |
| Quitter l'appli | **START** (deux fois si des minuteurs tournent) | |

Dans la liste des recettes :

| Action | Boutons physiques | Écran tactile |
|---|---|---|
| Choisir une recette | **↑** / **↓** | |
| Ouvrir la recette choisie | **A** | toucher sa ligne |
| Page précédente / suivante | **L** / **R** (ou **←** / **→**) | boutons **Haut** / **Bas** |
| Télécharger le menu (synchroniser) | **Y** | bouton **Synchro** |
| Réglages : adresse, micro, sensibilité | **X** | bouton **Réglages** |
| Recette d'exemple (minuteur de test de 10 s) | **SELECT** (ou **A** sans recette) | bouton **Recette d'exemple** |

Les commandes sans les mains (souffle dans le micro, main devant la caméra) arriveront aux étapes 5 et 6.

---

## 3. Organisation des fichiers

```
app3ds/
├── Makefile                 commandes make (voir section 6)
├── README.md                ce fichier
├── source/                  code de l'appli
│   ├── main.c               démarrage, boucle principale (60 images par seconde), passage
│   │                        d'un écran à l'autre (liste ↔ recette), arrêt
│   ├── vue_liste.c / .h     écran « menu de la semaine » et écran de synchronisation
│   ├── vue_recette.c / .h   écran « recette » : étape, ingrédients, portions
│   ├── vue_reglages.c / .h  écran Réglages : adresse, micro, sensibilité, jauge en direct
│   ├── vue_minuteurs.c / .h bandeau des minuteurs, alerte « C'est prêt ! », boutons
│   ├── micro.c / .h         capture du micro (service mic, tampon circulaire)
│   ├── dessin.c / .h        couleurs, texte, boutons, communs aux deux écrans
│   ├── alarme.c / .h        son (ndsp ou csnd), LED de notification, veille
│   ├── reseau.c / .h        téléchargement HTTP (httpc) et état du Wi-Fi (ac)
│   ├── saisie.c / .h        clavier de la console pour saisir l'adresse
│   ├── minuterie.c / .h     minuteurs en parallèle, sonnerie, double toucher (portable)
│   ├── souffle.c / .h       reconnaissance du souffle dans le son du micro (portable)
│   ├── synchro.c / .h       menu + recettes modifiées → cache de la carte SD (portable)
│   ├── menu.c / .h          menu de la semaine, jours, identifiants sûrs (portable)
│   ├── config.c / .h        réglages : 3ds/recettes/config.ini (portable)
│   ├── fichiers.c / .h      lecture et écriture atomique sur la carte SD (portable)
│   ├── catalogue.c / .h     parcours du cache de la carte SD (portable)
│   ├── recette_json.c / .h  JSON de la passerelle → structure Recette (portable)
│   ├── json.c / .h          analyseur JSON sans allocation (portable)
│   ├── recette.c / .h       structure d'une recette + la recette d'exemple en dur (portable)
│   ├── echelle.c / .h       calcul des quantités selon les portions (portable)
│   └── mise_en_page.c / .h  découpage du texte en lignes, choix de la taille (portable)
└── tests/                   tests qui tournent sur le Mac
    ├── Makefile
    ├── test_echelle.c       mêmes cas que passerelle/tests/test_echelle.py
    ├── test_mise_en_page.c  découpage en lignes, troncature, accents, mots trop longs
    ├── test_json.c          JSON valides et invalides, échappements, 20 000 essais au hasard
    ├── test_recette_json.c  lecture d'une recette, limites, dossier de carte SD simulé
    ├── test_synchro.c       réglages, menu, synchronisation contre un faux serveur
    ├── test_minuterie.c     minuteurs avec une horloge factice
    ├── test_souffle.c       souffle, voix, claquement, bruit de cuisine simulés
    ├── verifier_dossier.c   outil : lire tout un dossier de recettes (voir section 8)
    ├── synchro_hote.c       outil : vraie synchronisation depuis le Mac (voir section 8)
    ├── verif.h              mini-outil de vérification
    └── stubs/               FAUX en-têtes 3DS, seulement pour vérifier la syntaxe
```

**Qu'est-ce qu'un module « portable » ?** C'est un fichier qui n'utilise aucune fonction propre
à la 3DS : `echelle.c`, `mise_en_page.c`, `recette.c`, `json.c`, `recette_json.c`,
`catalogue.c`, `fichiers.c`, `config.c`, `menu.c`, `synchro.c`, `minuterie.c` et `souffle.c`. Même la synchronisation est
portable : le téléchargement lui est passé en paramètre (httpc sur la console, un faux serveur ou
curl sur le Mac). On peut donc le compiler et le tester sur
le Mac avec le compilateur habituel (clang), sans devkitPro.

**Pourquoi le calcul des portions existe deux fois ?** La passerelle (en Python) et l'appli (en C)
doivent arrondir exactement de la même façon. Par exemple, 135 g × ½ donne 70 g des deux côtés,
et non 67,5 g d'un côté et 70 g de l'autre. Les règles sont écrites dans
[../docs/schema.md](../docs/schema.md), et les deux séries de tests vérifient les mêmes cas.

**Pourquoi des tailles fixes ?** Une recette occupe une zone mémoire de taille fixe (environ 40 Ko) :
au plus 48 ingrédients et 32 étapes de 1 280 octets chacune (la plus longue étape de la base en
fait 941), etc. On évite ainsi l'allocation dynamique,
une source classique de plantages sur une console qui a peu de mémoire. Les limites sont
regroupées en haut de `source/recette.h`.

---

## 4. Ce qu'il faut avant de commencer

- **Sur la console** : une 3DS avec **Luma3DS** (déjà le cas) et le **Homebrew Launcher**.
  Avec Luma3DS, on l'ouvre en général depuis l'appli *Téléchargement* (Download Play), si elle a
  été réglée comme lanceur dans le menu Rosalina. Pour ce réglage : **L + ↓ + SELECT**, puis
  *Miscellaneous options*, puis *Switch the hb. title to the current app*.
- **Sur le Mac** : devkitPro, la chaîne de compilation pour la 3DS (installé, voir la section 5).
- **Réseau** : le Mac (ou le NAS) et la 3DS sur le **même réseau Wi-Fi**. Il sert à envoyer
  l'appli et à télécharger les recettes : on ne sort jamais la carte SD.

---

## 5. Installer devkitPro sur le Mac (une seule fois)

devkitPro fournit le compilateur ARM pour la 3DS (devkitARM), libctru, citro2d, et l'outil
d'envoi par Wi-Fi (3dslink). L'installation demande ton **mot de passe administrateur**.

1. **Installer le gestionnaire de paquets de devkitPro.**
   Télécharger `devkitpro-pacman-installer.pkg` sur
   <https://github.com/devkitPro/pacman/releases/latest>, puis l'ouvrir et suivre l'installateur.
   Si macOS bloque le fichier, aller dans *Réglages Système*, puis *Confidentialité et sécurité*,
   et cliquer sur *Ouvrir quand même*.

2. **Installer les outils 3DS.** Dans un terminal :
   ```bash
   sudo dkp-pacman -Sy 3ds-dev
   ```
   La commande propose une liste de paquets : appuyer sur **Entrée** pour tout prendre, puis
   répondre **Y** pour confirmer. Le tout fait quelques centaines de Mo.

3. **Indiquer au terminal où sont les outils.** Ouvrir le fichier `~/.zshrc`, par exemple avec la
   commande `open -e ~/.zshrc`, et ajouter ces trois lignes à la fin :
   ```bash
   export DEVKITPRO=/opt/devkitpro
   export DEVKITARM=$DEVKITPRO/devkitARM
   export PATH=$DEVKITPRO/tools/bin:$PATH
   ```
   Enregistrer, puis **fermer et rouvrir le terminal**.

4. **Vérifier l'installation.** Ces deux commandes doivent répondre sans erreur :
   ```bash
   echo $DEVKITARM
   ```
   Elle doit afficher `/opt/devkitpro/devkitARM`.
   ```bash
   $DEVKITARM/bin/arm-none-eabi-gcc --version
   ```
   Elle doit afficher une version de gcc.

---

## 6. Compiler l'appli

Toutes les commandes se lancent dans le dossier `app3ds/` :

```bash
cd ~/Documents/Code/3DS/app3ds
```

| Commande | Ce qu'elle fait | devkitPro requis |
|---|---|---|
| `make` | compile l'appli et produit **`recettes.3dsx`** | oui |
| `make envoyer` | compile puis envoie l'appli à la console par Wi-Fi | oui |
| `make test` | lance les tests sur le Mac | non |
| `make clean` | supprime les fichiers compilés | oui |

Lancer la compilation :

```bash
make
```

Si tout se passe bien, les dernières lignes affichées mentionnent `recettes.3dsx`. Les fichiers
intermédiaires sont dans `build/`, qu'on peut supprimer sans risque.

---

## 7. Essayer l'appli

### Sur la console, par Wi-Fi (conseillé)

1. Sur la 3DS, ouvrir le **Homebrew Launcher**.
2. Appuyer sur **Y** : l'écran affiche *waiting for 3dslink* avec l'adresse IP de la console,
   par exemple `192.168.1.42`.
3. Sur le Mac, en remplaçant l'adresse par celle affichée sur la console :
   ```bash
   make envoyer IP=192.168.1.42
   ```
   Sans `IP=…`, 3dslink cherche la console tout seul sur le réseau. Ça marche souvent, mais pas
   toujours.
4. L'appli démarre aussitôt sur la console. **START** la quitte et ramène au Homebrew Launcher.

### Sur la console, par la carte SD

Copier `recettes.3dsx` dans le dossier `3ds/` de la carte SD, ce qui donne
`sdmc:/3ds/recettes.3dsx`. L'appli apparaît ensuite dans la liste du Homebrew Launcher.

### Sur le Mac, dans l'émulateur Azahar

Installer [Azahar](https://azahar-emu.org/), puis ouvrir `recettes.3dsx` avec *Fichier*, puis
*Charger un fichier*. C'est pratique pour vérifier vite l'affichage. Le micro, la caméra, la LED et
la mise en veille ne se vérifient que sur la vraie console.

### Ce qu'il faut vérifier à l'étape 1

- [ ] Les accents s'affichent bien : « é », « è », « à », « ç », « œ » (dans « œuf »), « ° » (dans « 200°C »).
- [ ] Les fractions et le signe fois s'affichent bien : « ½ », « ¼ », « × ».
- [ ] Le texte de l'étape est lisible à environ 50 cm, console posée sur le plan de travail.
- [ ] La dernière étape, la plus longue, tient entièrement à l'écran.
- [ ] Les boutons **Précédent**, **Suivant**, **-** et **+** réagissent au doigt.
- [ ] À ×2, « 220 g de poisson blanc » devient « 440 g de poisson blanc ».
- [ ] La liste d'ingrédients défile avec ↑ / ↓.

Si un caractère s'affiche mal (carré vide ou point d'interrogation), noter lequel : il faudra le
remplacer, ou embarquer une police.

### Brancher la console sur la passerelle (étapes 2 et 3)

Tout se fait par Wi-Fi.

1. **Démarrer la passerelle.** En attendant le NAS, elle peut tourner sur le Mac :
   ```bash
   cd ~/Documents/Code/3DS/passerelle && make lancer
   ```
   Le terminal affiche une ligne *Adresse à saisir sur la 3DS : http://192.168.1.84:8383*.
   Laisser ce terminal ouvert pendant les essais. Au premier lancement, macOS peut demander
   d'**autoriser les connexions entrantes** pour Python : répondre *Autoriser*.
2. **Composer le menu.** Sur le téléphone ou le Mac, ouvrir cette adresse dans le navigateur,
   chercher des recettes et toucher **Ajouter**. L'adresse à saisir sur la 3DS est rappelée en
   haut de la page. Avec Mealie, ce sont les repas du planning des 7 prochains jours.
3. **Sur la console**, lancer l'appli (`make envoyer IP=…`), toucher **Adresse**, taper l'adresse
   affichée (`192.168.1.84:8383` suffit : `http://` est ajouté tout seul), puis valider avec **OK**.
   L'adresse est enregistrée dans `3ds/recettes/config.ini`, et la synchronisation démarre aussitôt.
4. Les fois suivantes, la synchronisation se fait **toute seule au démarrage**. Le bouton
   **Synchro** la relance, par exemple après avoir changé le menu.

### Ce qu'il faut vérifier aux étapes 2 et 3

- [ ] Premier lancement sans adresse : le message invite à toucher **Adresse**, et
      **Recette d'exemple** fonctionne.
- [ ] Le clavier de la console s'ouvre avec **Adresse** ; une adresse invalide (par exemple
      `https://…`) est refusée avec un message clair.
- [ ] Après la saisie, l'écran *Synchronisation* défile, puis le menu apparaît avec un message
      *Synchronisé : 3 recettes au menu, 3 téléchargées.*
- [ ] Un second **Synchro** ne retélécharge rien (*Synchronisé : 3 recettes au menu.*).
- [ ] Retirer une recette du menu sur la page web, puis **Synchro** : elle disparaît de la liste.
- [ ] **Hors ligne** : arrêter la passerelle (Ctrl+C dans le terminal), puis **Synchro**. Un
      message orange s'affiche après quelques secondes, et les recettes restent utilisables.
- [ ] Wi-Fi coupé (interrupteur ou mode avion) : message *Wi-Fi non connecté*, recettes utilisables.
- [ ] Toucher une ligne ouvre la recette ; **Liste** ou **SELECT** ramène à la liste.

### Ce qu'il faut vérifier à l'étape 4

- [ ] Dans la liste, **SELECT** ouvre la recette d'exemple. À l'étape 2, le bouton « Test de
      l'alarme 10 s » lance le minuteur, et le bandeau apparaît en bas de l'écran du haut.
- [ ] Au bout de 10 s : bip-bip répété, LED orange qui clignote, « C'est prêt ! » en bas.
- [ ] Un toucher ou un bouton coupe le son, la LED et l'alerte, sans changer d'étape.
- [ ] Deux minuteurs à la fois : lancer celui de l'étape 1 (15 min), puis celui de l'étape 2.
      Les deux s'affichent dans le bandeau.
- [ ] Toucher un minuteur qui tourne affiche « Arrêter ce minuteur ? », et un second toucher
      l'arrête ; sans second toucher, il continue.
- [ ] Minuteur en cours, console **refermée** quelques secondes : elle ne se met pas en veille,
      et le décompte a continué à la réouverture.
- [ ] **START** avec un minuteur en cours affiche « START encore pour quitter ».
- [ ] Le texte de l'étape reste entièrement visible au-dessus du bandeau.

### Ce qu'il faut vérifier à l'étape 5

- [ ] **Réglages** (X dans la liste) : la grande jauge bouge quand on parle ou qu'on souffle.
- [ ] Un souffle de face vers le micro, à 20-30 cm, dépasse le trait et affiche « Souffle détecté ! ».
      Sinon, monter la sensibilité.
- [ ] Parler normalement, ou poser une casserole, n'affiche pas « Souffle détecté ! ». Sinon,
      baisser la sensibilité.
- [ ] Dans une recette, un souffle passe à l'étape suivante, et un long souffle n'avance que d'une étape.
- [ ] Pendant l'alarme d'un minuteur, le bip ne fait pas avancer la recette.
- [ ] **Micro : coupé** dans les Réglages : la jauge disparaît de la recette, et souffler ne fait plus rien.
- [ ] Les réglages sont conservés après avoir quitté puis relancé l'appli.

`passerelle/outils/exporter_sd.py` (`make export-sd`) prépare encore des fichiers pour une copie
manuelle sur la carte, mais on n'en a plus besoin.

---

## 8. Les tests sur le Mac

```bash
make test
```

Cette commande fait huit choses, sans devkitPro :

1. **`test_echelle`** : 40 vérifications sur le calcul des quantités. Elle vérifie les arrondis,
   les fractions (« 1 ½ »), les pluriels (« 2 gousses ») et les plages (« 40-50 g »).
2. **`test_mise_en_page`** : 21 vérifications sur le découpage du texte en lignes et le
   raccourcissement des titres. Elle vérifie que les accents comptent pour un seul caractère,
   qu'un mot trop long ne bloque rien, et que la taille de police est bien choisie.
3. **`test_json`** : 62 vérifications de l'analyseur JSON. Elle couvre les documents valides et
   invalides, les échappements (`\u00e9`, émojis), les fichiers coupés en plein milieu,
   l'imbrication excessive, et 20 000 documents tirés au hasard qui ne doivent jamais le faire planter.
4. **`test_recette_json`** : 65 vérifications de la lecture d'une recette et du catalogue. Elle
   couvre les champs, les limites dépassées, et un faux dossier de carte SD avec des fichiers
   cassés, trop gros, et des fichiers `._` de macOS.
5. **`test_synchro`** : 80 vérifications des réglages (adresse, micro, sensibilité), du menu
   (jours de la semaine, identifiants piégés comme `../`) et de la synchronisation contre un faux
   serveur. Elle vérifie qu'une passerelle injoignable laisse la carte intacte, qu'une recette
   abîmée ne remplace pas la bonne, que rien n'est retéléchargé sans changement, et que le ménage
   est fait.
6. **`test_minuterie`** : 37 vérifications avec une horloge factice. Elle couvre le décompte
   arrondi à la seconde, la sonnerie déclenchée une seule fois, l'ordre d'affichage, le double
   toucher pour arrêter, 6 minuteurs au plus, et l'arrêt de toutes les sonneries.
7. **`test_souffle`** : 30 vérifications avec des sons simulés, envoyés bloc par bloc comme sur
   la console. Elle couvre :
   - le silence, le bruit de cuisine, un claquement et la voix, qui ne déclenchent pas ;
   - un souffle, qui déclenche ;
   - un long souffle, qui ne déclenche qu'une fois ;
   - deux souffles espacés, qui déclenchent deux fois ;
   - la pause après un déclenchement, la pause pendant l'alarme, et les sensibilités.
8. **Vérification de syntaxe** de `dessin.c`, `vue_liste.c`, `vue_recette.c`,
   `vue_minuteurs.c`, `vue_reglages.c` et `main.c`. Ces
   fichiers utilisent des fonctions 3DS : on les vérifie avec de **faux** en-têtes (`tests/stubs/`), qui reproduisent les signatures
   de libctru et de citro2d. Ça repère les fautes de frappe et les erreurs de type, mais
   **ne remplace pas** la vraie compilation avec `make`.

Les tests tournent avec AddressSanitizer et UBSan, qui signalent les débordements de mémoire
et les calculs indéfinis.

**Vérifier toute une base.** L'outil `verifier_dossier` lit tous les `.json` d'un dossier avec le
code de l'appli, et affiche pour chacun le nombre d'ingrédients, d'étapes et de minuteurs, ou
l'erreur :
```bash
make -C tests verifier_dossier && tests/verifier_dossier ../passerelle/sd/3ds/recettes/cache
```

**Synchroniser depuis le Mac.** L'outil `synchro_hote` lance la vraie synchronisation de l'appli
contre une passerelle qui tourne. Le téléchargement passe par curl au lieu de httpc. Il remplit
une fausse carte SD et affiche le menu obtenu :
```bash
make -C tests synchro_hote && tests/synchro_hote http://127.0.0.1:8383 /tmp/fausse-sd
```

---

## 9. En cas de problème

| Symptôme | Cause probable et solution |
|---|---|
| `DEVKITARM n'est pas défini`, ou `no such file or directory: /bin/arm-none-eabi-gcc` | Les lignes de la section 5, étape 3, sont absentes de `~/.zshrc`, ou le terminal a été ouvert avant leur ajout. Dans ce cas, lancer `source ~/.zshrc` ou ouvrir un nouveau terminal. |
| `citro2d.h: No such file or directory` | Les outils 3DS ne sont pas installés : relancer `sudo dkp-pacman -Sy 3ds-dev`. |
| `3dslink: command not found` | Le dossier `$DEVKITPRO/tools/bin` manque dans `PATH` (section 5, étape 3). |
| 3dslink ne trouve pas la console | Vérifier que le Mac et la 3DS sont sur le même Wi-Fi, que la console attend bien (**Y** dans le Homebrew Launcher), et passer l'adresse IP explicitement avec `IP=…`. Le pare-feu de macOS peut aussi bloquer l'envoi. |
| *Passerelle injoignable (…) : pas de réponse* | La passerelle tourne-t-elle (`make lancer`, ou le conteneur sur le NAS) ? L'adresse est-elle la bonne (bouton **Adresse**) ? La console et le Mac sont-ils sur le même Wi-Fi ? Sur le Mac, vérifier que le pare-feu autorise Python (*Réglages Système*, puis *Réseau*, puis *Coupe-feu*). |
| *Wi-Fi non connecté* | Activer le Wi-Fi de la console (interrupteur sur le côté, ou dans le menu HOME). |
| *la passerelle n'arrive pas à joindre Mealie (502)* | La passerelle répond mais Mealie non : vérifier le conteneur Mealie et le jeton d'API (README de la passerelle). |
| *Adresse invalide* | Taper seulement l'adresse et le port, par exemple `192.168.1.84:8383`. Pas de `https`, pas de chemin. |
| Le souffle ne fait rien | Vérifier dans **Réglages** que le micro est activé et que la jauge bouge. Souffler plus près, ou monter la sensibilité. |
| La recette avance toute seule | Baisser la sensibilité dans **Réglages**. Une hotte ou un mixeur très bruyant près de la console peut suffire. On peut aussi couper le micro. |
| Pas de son à la fin d'un minuteur | Vérifier le curseur de volume. Puis extraire le firmware DSP une fois avec **DSP1**, qui crée `sdmc:/3ds/dspfirm.cdc` (voir l'étape 4). La LED et l'écran signalent l'alarme dans tous les cas. |
| « fichier(s) illisible(s) » | Le message donne le nom du fichier et la raison. **Synchro** le remplace par une version saine. |
| Écran noir, puis retour au menu | Noter ce qui s'affiche et me le transmettre. Le plus utile est le texte d'erreur de Luma3DS, s'il y en a un. |
