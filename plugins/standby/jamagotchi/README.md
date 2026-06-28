# jamagotchi — Q-Tune Standby Plugin Sample

`jamagotchi.so` is a standby UI plugin that's a musical **virtual pet** ("jam" +
Tamagotchi). It's the showcase sample for combining the SDK's three richest
features — **persistent state** (`qt_state_*`), **live pitch in standby**
(`display_frequency`), and the **touchscreen** — into a real little game.

## How it plays (the loop)

Like a real Tamagotchi, the pet **hides its meters** and just lives on the idle
screen. Over time a hidden need (hunger, fun, cleanliness, health) drifts down and
the pet **calls for attention** with a speech bubble:

> *"I'm hungry! Play E to feed me."*

You answer by **playing the prompted note** (through buffered bypass) or, with no
guitar, by **tapping** the pet. That launches a short **full-screen mini-game**; the
score you get **proportionally fills** the need (70% → +70%). Ignore a call too long
and it becomes a **care mistake** — enough of those and the pet grows up into a
washed-up "Lounge Act" instead of a "Headliner" (or, from neglect, dies; tap to
start a new egg). Stages: **Rookie → Busker → Open Mic → Headliner** (good care) or
**Lounge Act** (neglect). It naps automatically when tired.

Two corner buttons: **Status** (peek at the hidden meters, like the real toy's
status screen) and **Games** (replay any mini-game for a small, capped reward).

## The five mini-games (one per need)

Each leans on a different thing pitch can tell us, and each has a **touch fallback**:

| Game | Need | Play it with pitch | …or touch |
|------|------|--------------------|-----------|
| **Note Match** | Feed | Play the shown note in tune | Tap the matching note |
| **Note Runner** | Jam | A **note attack** makes the pet jump obstacles | Tap to jump |
| **Pitch Steer** | Clean | **Low notes → left, high → right** to sweep the mess | Tap where to move |
| **Road Dash** | Heal | Hop the pet up across lanes of traffic — **a note attack hops it forward**; dodge the cars | Tap to hop |
| **Echo** | Settle | Tiles flip to reveal a 4-note sequence, then flip back; **play the pad notes in order** to fill them (Wordle-style) | Tap the pad in order |

### Pitch-selectable menus

Every choice (the call prompt, the results screen's **Try Again / Continue**, the
**Games** menu) can be picked by **playing an assigned note** or tapping. The Games
menu maps to the open strings (**Feed = E, Jam = A, Clean = D, Heal = G, Echo = B**);
two-button screens use **Continue = E, Try Again = A**. There's a brief input
lockout after each screen change so the note you just played doesn't auto-select.

## Buffered bypass

The pet "hears" your guitar via `display_frequency`, which delivers notes reliably in
**Buffered Bypass**. If no notes are arriving it shows a *"works best in Buffered
Bypass"* tip and you can do everything by **touch**.

## How it's built

- **`main/jama_core.{c,h}` + `main/jama_config.h`** — the game model (needs, calls,
  care mistakes, evolution) as **pure C with no LVGL/firmware deps**, so it's
  unit-tested on the host. All balancing is in `jama_config.h`.
- **`main/jama_quiz.{c,h}`** — pure-C mini-game kernels (note matching, pitch→
  position, sequence check, in-tune test), also host-tested.
- **`main/jamagotchi.cpp`** — the LVGL UI + glue: a small scene state machine
  (pet / game / result / menu / status), the five games, pitch routing, and
  `qt_state_*` persistence.
- **`test/run.sh`** — builds + runs the host unit tests (no device needed).

### Run the tests
```sh
./test/run.sh        # jama_core + jama_quiz unit tests
```

## Persistence & flash wear

The pet (`jama_state_t`) is saved under one key in this plugin's private
`qt_state_*` store, surviving standby↔tuner switches and power cycles. Writes are
RAM-cheap; `commit()` (flash) happens only on a slow cadence and the firmware
auto-commits when you leave the screen. `display_frequency` never commits.

## Build & upload

```sh
./docker-build.sh plugins/standby/jamagotchi      # writes build/jamagotchi.so, then validates
```
> If you change `sdkconfig.defaults` (fonts/widgets), `rm -rf build sdkconfig` first.

Copy `build/jamagotchi.so` to the pedal's `/plugins`, restart, and select
**Jamagotchi** under **Settings → Display → Standby Screen**. No guitar? Enable
*Settings → Advanced → Developer Tools → Test Signal* to drive the games from the
built-in sweep. Reset the pet via *Settings → Plugin Data*.

## Identity / customising

`uid = "qtune.jamagotchi.plxzk4"` keys the saved pet — never change it after
publishing. Tune the game in `main/jama_config.h`; add a life stage via
`jama_stage_t` + `jama_stage_name`. Never call `lvgl_port_lock()`; the `lv_timer`
is deleted in `cleanup()`.

## Artwork

The pet is drawn from the sprite pack in `main/assets/` — three stacked A8
silhouettes per frame, composited **body → face → hair** (hair last, so it falls
over the eyes). At runtime the body recolors to the user's accent, the hair to
accent ×0.58, and the face stays near-black; sickness tints the body/hair green
and death swaps in the gravestone. Each life stage has its own body + hair, and
each mood (neutral/blink, happy/wink, hungry, sick, asleep/Zzz) its own face,
selected and animated in `apply_pet_art()` in `main/jamagotchi.cpp`.

The C arrays in `main/assets/c_src/` are generated from `main/assets/png/` by
`main/assets/convert_assets.py` (`pip install pillow`, then
`python3 convert_assets.py --size 80`); see `main/assets/README.md` for the layer
model. The build globs `main/` recursively, so the sprite `.c` files compile into
the `.so` automatically — re-run a clean build (`rm -rf build`) after regenerating.

The sprites are A8 (1 byte/pixel), and the loader caps each plugin's PSRAM
footprint (~512 KB shared), so **canvas size is the budget lever**: the set is
rasterized at **80×80** (`--size 80`) to land at ~370 KB. Don't bump it back to
96×96 (~480 KB of art) or the loader rejects the plugin with *"PSRAM budget
exceeded."* A4 would halve the bytes but won't work here — the firmware's
software renderer only recolors A8 alpha images, not A4.
