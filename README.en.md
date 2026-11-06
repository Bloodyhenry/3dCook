# 3dCook — hands-free recipes on the Nintendo 3DS

*[Français](README.md) · English*

Prop the 3DS up on the kitchen counter and follow a recipe one step at a time without touching
it: blow into the microphone (or wave a hand in front of the camera) to go to the next step.

| Folder | Contents |
|---|---|
| [app3ds/](app3ds/) | 3DS homebrew app in C (libctru, citro2d) — see its README |
| [passerelle/](passerelle/) | Python gateway server (Docker, Synology NAS) that reads Mealie and serves lightweight JSON to the 3DS |
| [docs/schema.md](docs/schema.md) | JSON format exchanged between the gateway and the 3DS |

The app only talks to the gateway, never directly to Mealie.

## Features

- Large step text that resizes to fit the 400 × 240 top screen
- Portion scaling on the touch screen
- Several timers at once, with a sound alarm, a blinking LED and sleep mode blocked while cooking
- Next step by blowing into the microphone; hand-wave detection with the camera is in progress
- The menu comes from your Mealie favorites (or its meal planner) and syncs over Wi-Fi; recipes are kept on the SD
  card so you can cook offline

## Quick start

```bash
# Gateway (tests)
cd passerelle && make test

# 3DS app (needs devkitPro): tests on the Mac, then send to the console over Wi-Fi
cd app3ds && make test && make envoyer IP=192.168.1.xx
```

No recipes are included: the gateway reads the ones from your own Mealie instance.

The code, comments and sub-READMEs are written in French.
