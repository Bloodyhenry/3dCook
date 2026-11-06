# 3dCook — recettes « mains sales » sur Nintendo 3DS

*Français · [English](README.en.md)*

On pose la 3DS sur le plan de travail et on suit une recette étape par étape, sans la toucher :
on souffle dans le micro (ou on passe la main devant la caméra) pour avancer.

| Dossier | Contenu |
|---|---|
| [app3ds/](app3ds/) | Appli homebrew 3DS en C (libctru, citro2d) — voir son README |
| [passerelle/](passerelle/) | Serveur Python (Docker, NAS Synology) qui lit Mealie et sert un JSON léger à la 3DS |
| [docs/schema.md](docs/schema.md) | Format JSON échangé entre la passerelle et la 3DS |

L'appli ne parle qu'à la passerelle, jamais directement à Mealie.

## Démarrage rapide

```bash
# Passerelle (tests)
cd passerelle && make test

# Appli 3DS (devkitPro requis) : tests sur le Mac, puis envoi à la console par Wi-Fi
cd app3ds && make test && make envoyer IP=192.168.1.xx
```

Aucune recette n'est fournie : la passerelle lit celles de votre propre instance Mealie.
