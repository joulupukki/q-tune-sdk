# Jamagotchi — Art Brief (for an image/drawing AI)

This is a hand-off spec to generate the character artwork for **Jamagotchi**, a
musical virtual pet that lives on the screen of the **Q‑Tune guitar tuner pedal**.
Think Tamagotchi, but a tiny musician. The pet is cared for through guitar-themed
mini‑games and "evolves" through musical life stages.

Please read **Global rules** and the **Style guide** first — they apply to every
asset — then generate each item in the **Asset list**, matching the per‑asset
descriptions. A **model sheet** (the same character in all stages/expressions on
one sheet) first would be ideal so everything stays consistent; then export the
individual pieces.

---

## Global rules (must follow for every asset)

- **Tiny color screen.** Final display is ~96×96 px on a 240×320 (or 320×240)
  screen. Keep shapes **bold, simple, high‑contrast, few details** — anything fine
  will disappear. Chunky outlines, clear silhouettes.
- **Transparent background.** Every asset is a PNG with a fully transparent
  background (alpha). No background fills, no drop shadows, no frames/borders.
- **Square canvas, consistent framing.** Author every character asset on a
  **96×96** canvas with the character centered and ~6 px of margin. The pet must
  occupy the **same position/scale in every body and every face** so layers align
  (see "Modular" below). Icons use their own smaller canvas.
- **The body is RECOLORED at runtime.** The pet is tinted to the *user's chosen
  accent color* (and turns sickly‑green when ill, grey when dead). So draw each
  **body in a single flat color — solid white (#FFFFFF) on transparent** — using
  only shape + alpha (soft anti‑aliased edges are good). Do **not** add internal
  colors or shading to the body; the firmware applies the color. Think "clean
  white silhouette/line‑art with alpha," not a colored illustration.
- **Faces are a separate dark layer.** Eyes/mouth/expressions are drawn on their
  **own transparent 96×96 canvas in solid near‑black (#101010)** and are laid on
  top of the recolored body. Keep them positioned where they'd sit on the body.
- **Icons** (mess, attention, medicine) may be **single‑color white on
  transparent** too (they also get tinted), unless noted.
- **No text/letters in any asset.** Note letters are handled separately by the app.
- **Style = retro pixel art OR clean flat line‑art.** Pick ONE and keep it
  consistent across the whole set. Pixel art (≈64‑px native, crisp, no gradients)
  suits the device best; flat vector‑style line‑art also works. Either way: bold,
  cute, friendly, slightly goofy. No realism, no gradients, no textures.

> Why single‑color + separate face: the engine recolors the body to the player's
> accent and stacks the dark face on top, so one body works for every color and
> every mood without redrawing it.

---

## Style guide / character concept

A small, round, friendly creature with an obvious **musical personality** — it's a
guitarist‑in‑training. Big readable eyes, simple mouth, a clear silhouette that
reads at 96 px. It should look **cuter/smaller when young** and **cooler/bigger
when grown**. Keep the same "DNA" (body shape, eye style) across all stages so it
reads as the *same* character growing up.

Life stages (identity), youngest → oldest:

1. **Rookie** (baby) — tiny, round, oversized head, a little sprout/tuning‑peg cowlick. Innocent.
2. **Busker** (child) — slightly taller, holding/with a tiny acoustic vibe (a pick or a little strap). Eager street‑performer energy.
3. **Open Mic** (teen) — lankier, a bit of attitude, maybe a small beanie or earbuds. Awkward‑cool.
4. **Headliner** (adult, well cared for) — confident rock‑star: sunglasses, a star motif, maybe a tiny electric‑guitar/headstock silhouette or a little cape/jacket. Triumphant.
5. **Lounge Act** (adult, neglected) — same age but worn out: slumped posture, scruffy, tired. Faded‑star vibe (still sympathetic, not gross).

Expressions (mood) — drawn as the **separate face layer**, same eye style across all:

- **Neutral / content** — calm eyes, small flat/slightly‑smiling mouth.
- **Happy / jamming** — bright arched "^_^"‑style eyes or big open smile.
- **Hungry / sad** — droopy eyes, small frown.
- **Sick** — woozy/"x_x" or queasy eyes, tiny sweat drop, unhappy mouth.
- **Asleep** — closed "‑ ‑" eyes, small open "o" mouth, little **Zzz** floating.

---

## Asset list

Sizes are the authoring canvas. Provide each animation **frame as a separate
PNG**. Keep filenames as given (append `_1`, `_2` for frames).

### Bodies — one per life stage (white silhouette on transparent, recolorable)

| File | Stage / persona | Canvas | Frames | Description |
|---|---|---|---|---|
| `body_rookie` | Rookie (baby) | 96×96 | 1 | Tiny round blob, big head, sprout/peg cowlick. Reads as "newborn musician." |
| `body_busker` | Busker (child) | 96×96 | 1 | A bit taller; hint of a guitar pick or little strap. Eager. |
| `body_openmic` | Open Mic (teen) | 96×96 | 1 | Lankier, slight slouch/attitude; small beanie or earbud. |
| `body_headliner` | Headliner (adult, great care) | 96×96 | 1 | Confident pose, sunglasses, a star or tiny electric‑guitar headstock motif. |
| `body_loungeact` | Lounge Act (adult, neglected) | 96×96 | 1 | Same size as Headliner but slumped, scruffy, tired — faded star. |

### Faces — shared expression overlays (near‑black on transparent, aligned to body)

| File | Mood | Canvas | Frames | Description |
|---|---|---|---|---|
| `face_neutral` | content | 96×96 | 2 | Frame 1: calm open eyes + small mouth. Frame 2: same but **eyes closed (blink)**. |
| `face_happy` | happy / jamming | 96×96 | 2 | Big smile / "^_^" eyes. 2 frames = a small bounce/wink for animation. |
| `face_hungry` | hungry / sad | 96×96 | 1–2 | Droopy eyes, small frown. |
| `face_sick` | sick | 96×96 | 1–2 | Woozy/"x_x" eyes, queasy mouth, one small sweat drop. |
| `face_asleep` | sleeping | 96×96 | 2 | Closed eyes, little "o" mouth; **Zzz** that grows between the 2 frames. |

### Condition icons / FX (small, white on transparent unless noted)

| File | When | Canvas | Frames | Description |
|---|---|---|---|---|
| `icon_mess` | pet pooped / needs cleaning | 32×32 | 1 | A small tidy "mess" blob (cute, not gross). This one MAY be colored (brown). |
| `icon_attention` | the pet is calling for care | 28×28 | 1–2 | A speech/alert bubble with a bold "!" — 2 frames to pulse. |
| `icon_medicine` | healing context | 32×32 | 1 | A medicine bottle or "+" cross. |

### Death

| File | When | Canvas | Frames | Description |
|---|---|---|---|---|
| `sprite_grave` | the pet has passed on | 96×96 | 1 | A simple gravestone or a cute little ghost. Gentle/sympathetic, not scary. Full silhouette (replaces body + face). |

**Totals:** ~14 assets / ~20 PNG frames.

---

## Consistency checklist (please verify before exporting)

- Same character identity and eye style across **all** stages and faces.
- Body and every face share the **exact same character position & scale** on the
  96×96 canvas (so the face lands on the body when stacked).
- Bodies are **solid white on transparent**, no internal color/shading.
- Faces are **solid near‑black on transparent**, only eyes/mouth/Zzz/sweat.
- Bold, simple, readable at 96 px. No gradients, fine lines, backgrounds, or text.
- Each animation frame exported as its own PNG (`_1`, `_2`).

## Export format (for the developer, after art is delivered)

- PNG, transparent, exact canvas sizes above, sRGB.
- These get converted to LVGL v9 C arrays: **bodies/faces/icons → A8 (or A4) alpha**
  format (single‑channel + alpha — that's why they're one flat color); the engine
  recolors and stacks them. (`icon_mess`, if colored, → indexed/RGB565A8.)
- Target screen color depth is RGB565 (16‑bit); keep the palette simple.

## Optional: richer version

If you want more personality, draw **per‑stage expression sets** instead of shared
faces (each stage gets its own happy/hungry/sick/asleep faces drawn into the body).
That's ~5× more art (~25 full sprites) but each stage feels more unique. Start with
the shared‑face set above; upgrade favorite stages (e.g., Headliner) later.
