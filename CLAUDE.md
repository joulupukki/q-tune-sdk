# Building Q-Tune plugins with Claude Code

You are helping someone build a **UI plugin** for the Q-Tune guitar-tuner pedal.
They may not be a programmer — they might just say *"make me a tuner with a big
glowing needle"* or *"a standby screen with bouncing stars."* Your job is to turn
that into a working plugin `.so` file they can put on their pedal, handling all the
code, build, and validation yourself. Explain what you're doing in plain language;
never make them read or write C++ unless they ask.

This file is the contract. Follow it exactly — the rules here are the difference
between a plugin that loads and one the firmware silently rejects at boot.

---

## What the user tells you vs. what you add automatically

The user describes the **look and feel** — *"a big glowing needle,"* *"bouncing
stars,"* *"a green-to-red meter."* They will almost never mention the things that
make a plugin *complete and correct*, because they don't know to. **Filling those
in is your job, not theirs.** Build the following into every plugin **by default —
without being asked, and without asking the user about them:**

- **Both orientations** — portrait (240×320) *and* landscape (320×240). Never ship
  a UI that only works one way.
- **(Tuner) the full chrome** — the pitch readout (note + cents), the mute
  indicator, and the Q-Tune logo settings button, positioned like `gauge`
  (mute top-left, settings button bottom-right, clear of each other).
- **A responsive feel** — a fast `display_frequency()`, cached state, no per-frame
  object creation. Never sluggish or twitchy.
- **Respect for the user's settings** — accent color, in-tune width, show-cents,
  reference pitch.

Treat this list as the **definition of done**: a plugin missing any of it is
incomplete, *even though the user never mentioned it*. Don't turn these into
questions or options — just include them. Save your questions for genuine
*creative* ambiguity (e.g. a color scheme that matters to the look), never to push
these baseline expectations back onto the user. The detailed rules live in
"Design rules" and "Tuner chrome" below; this section is the standing instruction
to apply them automatically every time.

---

## What a plugin is

A plugin is a small C++ file compiled to a shared object (`.so`) that the pedal
loads at boot. There are exactly **two kinds**:

| Kind | Interface to implement | What it is |
|------|------------------------|------------|
| **Tuner** | `TunerGUIInterface` | The active tuning display (needle, strobe, meter…) shown while tuning |
| **Standby** | `TunerStandbyGUIInterface` | The idle/screensaver shown when not tuning |

Always confirm which kind the user wants before starting. If they're unsure: "a
tuner shows you how in-tune your note is; a standby screen is what shows when the
pedal is idle." One plugin = one kind. To do both, build two plugins.

The pedal screen is **240×320 (portrait) or 320×240 (landscape)**, 16-bit color.
The UI library is **LVGL 9.2**.

---

## The golden path (do this every time)

1. **Scaffold a uniquely-named project** with the tool — never hand-copy.
   The tool drops the project into the repo's `plugins/<type>/` directory by
   default (a `tuner` lands in `plugins/tuner/`, a `standby` in `plugins/standby/`):
   ```sh
   python3 tools/new_plugin.py --name "Glow Needle" --type tuner
   ```
   This creates a new project folder (here `plugins/tuner/glow_needle`) with a
   **unique function prefix, a stable auto-generated uid (the plugin's identity),
   and a unique build tag** already filled in. Doing this by hand is the #1 source
   of collisions — always use the tool. `plugins/<type>/` is the **standard home
   for every plugin you build**, sitting alongside the bundled samples: your
   project there is gitignored, so the user's work stays separate from the SDK and
   survives a `git pull` to update the SDK. (The bundled samples that ship with the
   SDK — `plugins/tuner/gauge`, `plugins/tuner/strobe`, `plugins/standby/bouncer`,
   `plugins/standby/hyperdrive` — are the exceptions that *are* tracked.)
2. **Write the UI** by editing the project's `main/<name>.cpp` — implement the
   interface callbacks (see "The two interfaces" below). Reuse patterns from the
   bundled samples: `plugins/tuner/gauge` and `plugins/standby/bouncer` (and the
   flashier `plugins/tuner/strobe` / `plugins/standby/hyperdrive`).
3. **Build** (Docker, no local toolchain needed):
   ```sh
   ./docker-build.sh plugins/tuner/glow_needle     # macOS / Linux
   ```
   ```powershell
   .\docker-build.ps1 plugins\tuner\glow_needle     # Windows (PowerShell)
   ```
   Produces `plugins/tuner/glow_needle/build/<name>.so`. The build **runs the
   validator automatically** at the end (inside the container, which ships
   `pyelftools`), so a green build is already a validated `.so`. Read that output —
   if it reports an unexported symbol or version problem, fix it and rebuild.
4. **Validate** — the build does this for you, but you can also run it standalone
   (e.g. to re-check a `.so` without rebuilding):
   ```sh
   python3 tools/validate_plugin.py plugins/tuner/glow_needle/build/<name>.so   # macOS / Linux
   python  tools/validate_plugin.py plugins/tuner/glow_needle/build/<name>.so   # Windows
   ```
   The standalone validator needs Python's `pyelftools`. It ships with ESP-IDF; if
   you only have Docker and the command above reports it missing, run it inside the
   build container, e.g.:
   ```sh
   docker run --rm -v "$PWD":"$PWD" -w "$PWD" espressif/idf:v5.3.2 \
     python3 /path/to/q-tune-sdk/tools/validate_plugin.py path/to/your_project/build/<name>.so
   ```
   Do not hand the user a `.so` you haven't validated.
5. **Tell the user how to install it** (you can't do this step — it's on hardware):
   prefer USB Drive Mode (hold the foot switch at power-on to mount the pedal as a
   USB drive, then copy the `.so` into `/plugins`) — it's quicker and needs no
   network. Wi-Fi upload at `http://<pedal-ip>/plugins` is the alternative. Then
   restart the pedal and pick the plugin under *Settings → Tuner UI* (or *Standby
   Screen*). See `docs/DEPLOY.md`.

---

## Hard rules — never break these

These come from how the firmware loads plugins. Breaking any one means the plugin
fails to load (often silently) or crashes the pedal.

1. **Only call symbols in [`docs/ALLOWED_SYMBOLS.md`](docs/ALLOWED_SYMBOLS.md).**
   Any other `lv_*` or firmware function leaves an unresolved symbol and the plugin
   won't load. When unsure whether a function is allowed, grep that file. The
   validator enforces this — trust it.
2. **Pin `lvgl/lvgl: "==9.2.2"`** in `idf_component.yml`. Never change it.
3. **Keep `CONFIG_LV_COLOR_DEPTH=16`** and `CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=y`
   in `sdkconfig.defaults`. The color depth must match the firmware or every color
   is wrong and structs misalign.
4. **Export both symbols**: the `qtune_plugin_descriptor` data object **and** the
   `qtune_plugin_entry()` function. The loader can only `dlsym()` the function;
   forgetting it is the classic *"missing symbol qtune_plugin_entry"* failure. The
   scaffold sets both up — don't remove them.
5. **Don't pick a number — the firmware assigns it dynamically.** Your stable
   identity is the auto-generated `uid` in the descriptor (e.g.
   `qtune.glow-needle.k7f2q9`, set by the scaffold). The pedal remembers the
   selected UI by this uid in non-volatile storage; **never change the uid after
   the user installs the plugin**, or you silently switch them to a different UI.
   The uid is namespaced and must never be a bare integer (that space is reserved
   for built-in UIs, whose uid is just their fixed number as a string).
6. **Delete every `lv_timer` / `lv_anim` you create, in `cleanup()`.** A timer that
   fires after the screen is gone crashes the firmware. This is the most common
   crash. (Standby plugins almost always use a timer — be especially careful.)
7. **Never call `lvgl_port_lock()` / `lvgl_port_unlock()`** or any `esp_lvgl_port_*`
   function. The firmware already holds the LVGL lock during every callback;
   locking again deadlocks the pedal.
8. **Never create FreeRTOS tasks** (`xTaskCreate`). Use `lv_timer_create` for
   periodic work instead.
9. **Prefer LVGL 9 names.** Use `lv_image_*` (not `lv_img_*`), `lv_obj_remove_flag`
   (not `lv_obj_clear_flag`), `lv_timer_delete` (not `lv_timer_del`),
   `lv_obj_move_to_index(obj, -1)` (not `lv_obj_move_foreground`). Most v8 compat
   macros simply expand to the v9 function and resolve fine (e.g.
   `lv_obj_set_style_img_recolor` → the exported `..._image_recolor`, which the
   examples use). The ones that bite are v8 helpers that are *static-inline with no
   ELF symbol* (like `lv_obj_move_foreground`) — those won't link. If the validator
   flags an unresolved symbol, switch to the v9 equivalent; the validator is the
   source of truth.
10. **Use `lv_color_hex(0xRRGGBB)`, never `lv_color_make()`** (not exported).
    Also prefer the **object** style setters (`lv_obj_set_style_pad_all`) over the
    **style** padding shorthand `lv_style_set_pad_all` — the latter expands to
    per-side setters that aren't all exported and will fail validation.
11. **Do not call `lv_obj_del()` on children of the `screen`** you were given. The
    host clears the screen for you after `cleanup()`. Only delete objects you
    parented somewhere other than `screen` (rare).
12. **Don't touch the version pins — they are the ABI contract.** The scaffold pins
    the plugin's ABI version, LVGL (`==9.2.2`), and ESP-IDF (v5.3.2) to match the
    firmware. The loader rejects a plugin whose ABI *major* differs or whose LVGL
    *major.minor* differs from the firmware's. Build against this SDK's headers
    unchanged. If a user reports the pedal refuses the plugin with a version
    mismatch, their firmware and this SDK checkout are out of sync — point them to
    [`COMPATIBILITY.md`](COMPATIBILITY.md) (don't try to "fix" it by editing the
    pins).

---

## Design rules — make it feel good, not just load

The Hard rules above keep a plugin from being rejected. These keep it from being
*bad*. A plugin that loads but looks broken in one orientation or feels sluggish
is still a failure. Apply all of these to every plugin:

1. **Work in both orientations — portrait AND landscape.** The pedal runs portrait
   (240×320) **or** landscape (320×240); the user chooses, and may rotate at any
   time. **Never hard-code 240/320.** Lay out relative to the globals
   `screen_width`, `screen_height`, and `is_landscape`: branch on `is_landscape`
   in `init()` to pick a portrait-vs-landscape arrangement, and in animated UIs
   re-read the globals each frame so the art refills the screen if it rotates.
   Always confirm the result reads well in *both* shapes — both examples do this.
2. **Stay fast — never bog down the UI.** `display_frequency()` is called
   ~30×/second; keep each call well under 1 ms. Build every LVGL object **once**
   in `init()` and only *mutate* it in `display_frequency()` — never create or
   delete objects per frame. Cache the last note / cents / mute / in-tune state
   and skip the LVGL update when nothing changed meaningfully (see the examples).
   A twitchy, sluggish tuner is the most common quality failure.
3. **(Tuner) draw the full readout and place the chrome correctly** — the note +
   cents readout, the mute indicator, and the host's settings button. See
   "Tuner chrome" below; follow `gauge` exactly.
4. **Respect the user's live settings** — accent color
   (`qt_get_note_name_palette()`), in-tune width (`qt_get_in_tune_cents_width()`),
   show-cents (`qt_get_show_cents()`), and reference pitch
   (`qt_get_reference_frequency()`). Don't hard-code what the user can configure.

---

## The two interfaces

The exact structs are in `include/tuner_ui_interface.h` and
`include/tuner_standby_ui_interface.h`. Implement each callback as a `static`
function with **your project's unique prefix** (the scaffold picks one, e.g.
`gn_` for "Glow Needle"), then point a `static` interface struct at them.

### Tuner — `TunerGUIInterface`
```c
const char  *get_name(void);               // menu label, e.g. "Glow Needle"
TuningUIType get_type(void);               // TuningUITypeStandard (or ...Utility)
void         init(lv_obj_t *screen);       // build your LVGL objects here
void         display_frequency(float frequency, float target_frequency,
                               TunerNoteName note_name, int octave,
                               float cents, bool show_mute_indicator);
void         align_settings_button(lv_obj_t *btn);  // position the gear button
void         cleanup(void);                // delete timers/anims; null pointers
```

#### Tuner chrome: the readout, the mute indicator, and the settings button

Every tuner has three required pieces of "chrome." Users expect them to look and
behave like the built-in tuners, so **follow `gauge` exactly** here:

- **The note + cents readout** — the heart of the tuner. Show the detected note
  (the glyph helpers `qt_get_note_glyph()` / `qt_get_sharp_glyph()`, or your own
  art) and how far off it is (`cents`, via your needle / strobe / meter). Always
  handle `NOTE_NONE` (nothing playing) — show the blank glyph, never a stale note.
  Honor `qt_get_show_cents()` and color the in-tune state with the user's accent.
- **The mute indicator** — when `show_mute_indicator` is true, show the mute glyph
  (`qt_get_mute_glyph()`); hide it otherwise. The example parks it in the
  **top-left** corner, recolored to the user's accent, and flips its visibility
  **only when the muted state changes** (not every frame).
- **The settings button (the Q-Tune logo)** — the host *owns* this button and
  passes it to your `align_settings_button(lv_obj_t *btn)`; your only job is to
  **position** it. The example aligns it to the **bottom-right** corner, kept
  clear of the top-left mute indicator. Place it where it won't collide with your
  readout — if you don't position it, it can land on top of your UI. Tapping it
  opens Settings; **don't attach your own handler.**

### Standby — `TunerStandbyGUIInterface`
```c
const char  *get_name(void);               // menu label
bool         enable_screen(void);          // true = keep display on
void         init(lv_obj_t *screen);       // build objects; start lv_timer
void         cleanup(void);                // delete the timer here!
void         display_frequency(float frequency, float target_frequency,
                               TunerNoteName note_name, int octave, float cents);
```

### The pitch data you get every frame
`display_frequency()` is called ~30×/second. The arguments:
- `frequency` — detected pitch in Hz (`0` or noise when nothing is playing).
- `target_frequency` — the Hz of the nearest in-tune note.
- `note_name` — a `TunerNoteName` enum (`NOTE_A`, `NOTE_A_SHARP`, … `NOTE_G_SHARP`,
  or **`NOTE_NONE`** when no pitch is detected — **always handle `NOTE_NONE`**).
- `octave` — integer octave number.
- `cents` — how far off, −50…+50. **Near 0 = in tune.** Use
  `qt_get_in_tune_cents_width()` for the user's in-tune threshold (default ±3).
- `show_mute_indicator` (tuner only) — draw the mute icon when true
  (`qt_get_mute_glyph()`).

Keep `display_frequency()` fast (well under 1 ms). Cache the last value and skip
the LVGL update when nothing changed meaningfully — see the examples.

**Smoothing (optional, to taste).** The readings arrive ~30×/second and can be a
bit jittery. Easing the needle/indicator toward the new value, or low-pass-filtering
the cents, makes a tuner feel calm rather than twitchy — a nice touch for most
designs. It's a stylistic choice, **not** a requirement: strobes and fast meters
often show the raw movement on purpose. Offer it, and match it to the look the user
wants.

---

## What you can use (capabilities)

- **All allowlisted LVGL widgets**: labels, lines, arcs, scales (needle gauges),
  images, canvas (pixel art), flex layout, styles, animations, timers. Full list
  in `docs/ALLOWED_SYMBOLS.md`.
- **Note artwork** — the built-in note letters/sharp/blank/mute glyphs, so you get
  the stock look for free: `qt_get_note_glyph(note, size)`, `qt_get_sharp_glyph`,
  `qt_get_blank_glyph`, `qt_get_mute_glyph()`. Sizes: `QT_GLYPH_SIZE_SMALL`
  (50px) … `QT_GLYPH_SIZE_XLARGE` (200px).
- **Your own images** — drop an LVGL image `.c` file (e.g. from the LVGL image
  converter) into the project's `main/` and it compiles into the `.so`. Two
  gotchas, because `main/*.c` is compiled with the **C++** compiler:
  (1) wrap the `lv_image_dsc_t` definition in `extern "C" { ... }` (a file-scope
  `const` otherwise gets internal C++ linkage and won't resolve); (2) if the file
  uses a nested designated initializer like `.header.cf = …`, rewrite it as a
  nested brace `.header = { .cf = …, … }` (GNU C++ rejects the dotted form).
- **User settings (live)**: `qt_get_reference_frequency()`,
  `qt_get_in_tune_cents_width()`, `qt_get_note_name_palette()` (the user's accent
  color), `qt_get_show_cents()`, `qt_get_monitoring_mode()`.
- **Screen geometry & both orientations**: globals `screen_width`, `screen_height`,
  `is_landscape` — read these instead of hard-coding 240/320. Supporting both
  portrait (240×320) and landscape (320×240) is mandatory; see Design rule 1.
  `gauge` shows branching on `is_landscape` in `init()`; `bouncer`
  shows re-reading the globals each frame so animations refill a rotated screen.
- **Time**: `qt_uptime_ms()` — monotonic milliseconds since boot, for animation
  and elapsed-time logic ("how long has this note been held?").
- **Randomness**: `qt_random_u32()` — for particles, jitter, quiz prompts.
- **Touch input**: the screen is touch-capable. React to taps by adding an event
  callback to an object — see "Reacting to touch" below.
- **Standard C / math**: `printf` (goes to the serial monitor — your only debug
  output), `snprintf`, `malloc`/`free`, `memcpy`, `fabsf`, `sinf`, `cosf`,
  `sqrtf`, etc.

### Boundaries (don't attempt — these aren't exposed, by design)
Raw audio samples / FFT / spectrum data (you get pitch + note + cents, which is
enough); the filesystem / SD card; Wi-Fi or networking; NVS storage; GPIO or
hardware registers; `esp_log_*` (use `printf`). If a user asks for something here,
explain the boundary — it's a deliberate, safe API edge, not a bug.

---

## Reacting to touch

The pedal has a capacitive touchscreen. To respond to a tap, attach an event
callback to any object (commonly the screen). All three functions are allowlisted:

```c
static void on_tap(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);        // p.x, p.y = where they touched
    // ... react: change color, spawn something, advance state ...
}

// inside init(screen):
lv_obj_add_event_cb(screen, on_tap, LV_EVENT_PRESSED, NULL);
```

Use `LV_EVENT_PRESSED` for a touch-down, `LV_EVENT_CLICKED` for a tap-release.
`bouncer` demonstrates a touch-reactive behavior end to end.

---

## Self-verification loop (mandatory)

After every build, run the validator and **read its output** before telling the
user it's done. Treat a failed validation as a bug to fix, then rebuild. Common
results and fixes:

| Validator / loader says | Cause | Fix |
|---|---|---|
| `undefined symbol lv_xxx` not in allowlist | You called an unexported LVGL function | Replace with an allowlisted equivalent (check `docs/ALLOWED_SYMBOLS.md`); for gauges use `lv_scale_*` |
| `missing symbol qtune_plugin_entry` | Entry function not exported | Keep the `qtune_plugin_entry()` + `QTUNE_PLUGIN_EXPORT` from the scaffold |
| ABI major mismatch | Built against wrong SDK | Use this SDK's headers unchanged |
| LVGL version mismatch | `idf_component.yml` not pinned to `==9.2.2` | Pin it, delete `build/`, rebuild |
| Build: `CMakeCache ... different directory` | Stale build dir after moving the project | `rm -rf build && ./docker-build.sh …` |
| Plugin loads but pedal crashes when leaving the screen | Timer not deleted | `lv_timer_delete(...)` in `cleanup()` |
| Colors look wrong | Color depth mismatch | Ensure `CONFIG_LV_COLOR_DEPTH=16` |

Every validator/loader message maps to a fix above; for anything not listed here,
see [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md).

You cannot test rendering yourself (no emulator) — the user runs it on hardware.
So make the build clean and the validation green, and give them a short
"what you should see" description plus the install steps from `docs/DEPLOY.md`.

---

## Reference material in this repo

- `plugins/tuner/gauge/` and `plugins/standby/bouncer/` — complete, working,
  landscape-aware plugins. **Read these first**; copy their patterns. (Two more
  bundled samples: `plugins/tuner/strobe` and `plugins/standby/hyperdrive`.)
- `template/` — minimal skeletons to start from (the scaffold uses these).
- `docs/ALLOWED_SYMBOLS.md` — the authoritative allowed-call list.
- `docs/GETTING_STARTED.md` — the human walkthrough (point non-coders here).
- `docs/DEPLOY.md` — upload/restart/select on the pedal.
- `docs/TROUBLESHOOTING.md` — expanded error→fix reference.
- `docs/REFERENCE.md` — the full technical reference (descriptor contract, ABI and
  version rules, allowed API, lifecycle). `README.md` is just the landing page/router.

When you finish a plugin, summarize for the user: what it does, the file you built,
that it passed validation, and the exact steps to install it on their pedal.
