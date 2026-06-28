# Jamagotchi Asset Pack — Q-Tune SDK drop-in

Character art for the Jamagotchi standby-screen pet. Drop this folder into the
plugin (e.g. `plugins/standby/jamagotchi/assets/`) and either commit the
pre-generated C arrays in `c_src/` or regenerate them with `convert_assets.py`.

## What's here

```
jamagotchi_assets/
├── png/                  # 56 source sprites (96x96 chars, smaller icons), transparent
├── c_src/                # LVGL v9 C arrays (A8) + jamagotchi_assets.h  [pre-generated]
├── manifest.json         # stage → body/hair/face mapping + layer/tint rules
├── render_manifest.json  # pet-state → sprite-stack + animation frame timing
├── convert_assets.py     # regenerates c_src/ from png/  (Pillow only)
└── README.md
```

## The layer model (critical)

Each pet frame is **three stacked layers**, composited **bottom → top in this order**:

1. `body_<stage>`  — A8 alpha. Recolor to the **user accent** color.  *(bottom)*
2. `face_<stage>_<mood>_<frame>` — A8 alpha. Near-black `#101010`, never tinted.  *(middle)*
3. `hair_<stage>`  — A8 alpha. Recolor to **accent × 0.58** (darker shade, stays
   visible on the black screen). On sick/dead, hair follows the body color × 0.58.  *(top)*

> **IMPORTANT — draw order:** hair is the **last** (topmost) layer, so it falls over
> the eyes/brow the way real hair does. The face is drawn **under** the hair. Do NOT
> draw the face after the hair. Order is **body → face → hair**.
> (This is intentional: e.g. openmic's side curtain partly covers one eye, headliner's
> bangs overlap the sunglasses. If you ever want an eye fully visible, that's an art
> tweak to the hair PNG, not a layer-order change.)

State color rules (body + hair together; face always near-black):
| state  | body color        | hair color            |
|--------|-------------------|-----------------------|
| normal | accent            | accent × 0.58         |
| sick   | sick-green #96AA5A| sick-green × 0.58     |
| dead   | use `sprite_grave`| (grave replaces stack)|

With LVGL v9, recolor an A8 image by setting the image style color:
```c
lv_obj_set_style_image_recolor(img, accent, 0);
lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
```
A8 images take the recolor as their RGB and use the stored alpha — exactly what
we want for one-color silhouettes.

## Stages & moods

Stages: `rookie → busker → openmic → headliner` (good care) | `loungeact` (neglect).
Moods (faces): `neutral`(+blink), `happy`(+wink), `hungry`, `sick`, `asleep`(+Zzz).
`headliner` also has a `cool` face (sunglasses) used as its idle/neutral look.

Hair styles (final, user-drawn): rookie = two sprouts; busker = Buddy-Holly;
openmic = Neil-Young curtains; headliner = Johnny-Rotten spikes; loungeact =
Brian-Wilson mop+beard. Sick faces are the wincing `>  <` tight-squint (not x_x).

## Build integration

1. Add `c_src/*.c` to your plugin's source list (CMake/Makefile glob).
2. `#include "jamagotchi_assets.h"` where you draw the pet.
3. Each sprite is a `const lv_image_dsc_t jama_<name>;`. Create three `lv_image`
   objects and add them to the pet container **in this z-order: body first (back),
   then face, then hair (front)** — LVGL draws later-added children on top:
   ```c
   lv_obj_t *body_img = lv_image_create(pet);   // added 1st -> back
   lv_obj_t *face_img = lv_image_create(pet);   // added 2nd -> middle
   lv_obj_t *hair_img = lv_image_create(pet);   // added 3rd -> front (over eyes)

   lv_image_set_src(body_img, &jama_body_busker);
   lv_image_set_src(face_img, &jama_face_busker_happy_1);
   lv_image_set_src(hair_img, &jama_hair_busker);
   ```
   (If you instead blit to one canvas, blit body → face → hair in that sequence.)
4. Recolor body to accent, hair to accent×0.58, leave face as-is.

### Regenerating C arrays
```
pip install pillow
python3 convert_assets.py --size 80
```
Re-run after editing any PNG. A8 is 1 byte/pixel, so canvas size sets the cost:
96×96 = 9216 B/sprite (full set ≈ 476 KB), 80×80 = 6400 B (≈ 332 KB), 72×72 = 5184 B.
The Q-Tune plugin loader caps each plugin's PSRAM footprint (~512 KB, shared), so the
SDK build uses `--size 80` to stay within budget — at 96×96 the loader rejects the
plugin with *"PSRAM budget exceeded."* (A4 would halve the bytes, but the firmware's
software renderer only recolors **A8** alpha images, not A4 — so keep these A8 and
shrink the canvas instead.)

## Notes
- `icon_mess` is provided white (tintable) AND `icon_mess_color` (brown ARGB8888); use
  whichever fits your HUD.
- All character sprites share the same 96×96 canvas & registration, so body/hair/face
  always align when stacked.
