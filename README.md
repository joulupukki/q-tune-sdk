# q-tune-sdk

The complete end-user SDK for writing UI plugins for the Q-Tune ESP32-S3 guitar
tuner pedal. A plugin is a small C/C++ module compiled to a relocatable shared
object (`.so`). The firmware loads it from `/data/plugins/` on internal flash at
boot and registers it alongside the built-in UIs.

Licensed under **Apache-2.0** (see [`LICENSE`](LICENSE)) — you are free to
develop and **sell** plugins built with this SDK.

---

## Start here

There are two ways to use this SDK. Pick the one that fits you:

### Path A — with Claude Code (recommended, little or no coding)

You describe the tuner or standby screen you want in plain English, and
[Claude Code](https://claude.com/claude-code) writes, builds, and validates the
plugin for you using this SDK. You only need [Docker](https://docs.docker.com/get-docker/)
installed. This repo ships a [`CLAUDE.md`](CLAUDE.md) that teaches the assistant
all the rules.

→ **Follow [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md).**

### Path B — by hand (for developers)

Scaffold a uniquely-named project, edit the C++, build, and validate yourself:

```sh
python3 tools/new_plugin.py --name "My Tuner" --type tuner
./docker-build.sh my_tuner
python3 tools/validate_plugin.py my_tuner/build/my_tuner.so
```

Then read the reference below for the descriptor contract, lifecycle, and the
full allowed API. The complete worked examples are in [`examples/`](examples/);
minimal starting points are in [`template/`](template/).

**Helpful docs:** [Getting Started](docs/GETTING_STARTED.md) ·
[Deploying to the pedal](docs/DEPLOY.md) · [Touch input](docs/TOUCH.md) ·
[Troubleshooting](docs/TROUBLESHOOTING.md) · [Allowed symbols](docs/ALLOWED_SYMBOLS.md) · [Serial console](docs/MONITOR.md)

> **Safety:** plugins are native code that runs unsandboxed on your pedal — see
> [Security](#12-security) before installing a plugin you didn't build.

---

## Table of Contents

1. [Plugin model overview](#1-plugin-model-overview)
2. [Prerequisites](#2-prerequisites)
3. [SDK directory layout](#3-sdk-directory-layout)
4. [The descriptor contract](#4-the-descriptor-contract)
5. [ABI and LVGL version rules](#5-abi-and-lvgl-version-rules)
6. [Plugin identity (the `uid`) and the numeric-ID pool](#6-plugin-identity-the-uid-and-the-numeric-id-pool)
7. [Allowed API surface](#7-allowed-api-surface)
8. [Plugin lifecycle](#8-plugin-lifecycle)
9. [Build → upload → activate](#9-build--upload--activate)
10. [Crash recovery and safe mode](#10-crash-recovery-and-safe-mode)
11. [Quick-start checklist](#11-quick-start-checklist)
12. [Security](#12-security)
13. [Disclaimer](#13-disclaimer)

---

## 1. Plugin model overview

```
  Plugin project (your code)
  ───────────────────────────
  example_tuner.cpp
      implements TunerGUIInterface
      exports   qtune_plugin_entry()  (+ descriptor)
        │
        │  idf.py build
        ▼
  example_tuner.so   ← position-independent, all internals hidden
        │
        │  Upload via http://<device-ip>/plugins in Wi-Fi mode.
        │
        │                          or
        │
        │  Use USB Drive mode (press and hold the foot switch
        │  at power up) and drop *.so file(s) into the /plugins
        │  directory.
        ▼
  /data/plugins/example_tuner.so   (internal flash, FAT32)
        │
        │  at boot: espressif/elf_loader reads .so,
        │           resolves undefined symbols against
        │           the firmware's export table,
        │           dlsym()s qtune_plugin_entry(),
        │           validates versions,
        │           registers the interface
        ▼
  Appears in Settings > Tuner UI (or Settings > Standby Screen)
```

The firmware binary is closed-source. Plugins interact with it only through:

- The LVGL 9.2.x API (widgets, animations, timers, styles).
- The `qt_get_*()` accessor functions and screen geometry globals declared in
  `include/qtune_plugin_host_api.h`.
- Standard C library functions (`printf`, `snprintf`, `memset`, `fabsf`, etc.)
  and `<math.h>`.

---

## 2. Prerequisites

A plugin must be built against the **exact** ESP-IDF (v5.3.2) and LVGL (9.2.x)
the firmware uses — mismatched versions corrupt memory at load time. The easiest
way to guarantee that is the bundled Docker environment, which pins everything
for you. Use the native install only if you already run ESP-IDF locally.

### Option A — Docker (recommended, zero local install)

Requires only [Docker](https://docs.docker.com/get-docker/) — works the same on
**macOS, Linux, and Windows**. The pinned `espressif/idf:v5.3.2` image carries
the whole toolchain, and the build validates the `.so` for you, so you need no
local ESP-IDF, Python, or `pyelftools`.

**macOS / Linux:**

```sh
# Build any plugin project (writes build/<name>.so, then validates it):
./docker-build.sh examples/example_tuner

# …or from inside a project directory:
cd examples/example_tuner && ../../docker-build.sh
```

**Windows** (PowerShell, with Docker Desktop running):

```powershell
.\docker-build.ps1 examples\example_tuner
```

On Windows, Docker Desktop must have access to the drive your SDK and project
live on — it does by default for your user profile; if you keep them elsewhere,
add the folder under *Docker Desktop → Settings → Resources → File sharing*. No
WSL setup is required (though building inside a WSL2 shell with the `.sh` script
also works if you prefer it).

For interactive development, open the repo in **VS Code → "Reopen in
Container"** (the `.devcontainer/` config builds the same pinned image and puts
`idf.py` on your `PATH`).

### Option B — native ESP-IDF install

| Tool / component | Version |
|------------------|---------|
| ESP-IDF          | v5.3.2 (exactly — the firmware is built with this) |
| IDF target       | `esp32s3` |
| Python           | 3.8 or newer |
| Host OS          | macOS, Linux, or Windows via WSL2 (native Windows ESP-IDF is unsupported here — use Option A instead) |

Follow Espressif's
[Get Started guide](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/get-started/index.html),
or, briefly:

```sh
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32s3
```

Then, in **every shell** before running `idf.py`, source the export script
(this sets `IDF_PATH` and activates the Python env):

```sh
. ~/esp/esp-idf/export.sh
```

### Optional: the offline validator

`tools/validate_plugin.py` (see §9) checks a built `.so` before you upload it.
It needs Python 3 and `pyelftools`.

If you build with ESP-IDF you already have both: the IDF Python environment
ships `pyelftools`, so just run the validator from a shell where you've sourced
`export.sh` — no install needed.

Otherwise install it into a virtual environment. (Don't run a bare
`pip install` — on many systems that resolves to an old Python 2, and Homebrew
/ Debian Python 3 block direct installs under PEP 668.)

```sh
python3 -m venv .venv && source .venv/bin/activate
pip install pyelftools
```

---

## 3. SDK directory layout

```
q-tune-sdk/
├── README.md                       This guide
├── LICENSE  /  NOTICE.md           Apache-2.0 (you may sell your plugins)
├── DISCLAIMER.md                   "As is", no warranty, use at your own risk
├── CONTRIBUTING.md  /  CODE_OF_CONDUCT.md
├── COMPATIBILITY.md                SDK ↔ ABI ↔ LVGL ↔ firmware version matrix
├── Dockerfile                      Pinned ESP-IDF v5.3.2 build environment
├── docker-build.sh / docker-build.ps1  One-step build+validate (macOS·Linux / Windows)
├── monitor.sh / monitor.ps1         Watch the pedal's serial console (zero install)
├── .devcontainer/                  VS Code "Reopen in Container" config
│
├── include/                        SDK headers — add this dir to your include path
│   ├── qtune_plugin.h              Umbrella header (include only this)
│   ├── qtune_plugin_abi.h          QTunePluginDescriptor (incl. uid), QTUNE_PLUGIN_EXPORT
│   ├── qtune_plugin_host_api.h     screen_width/height, qt_get_*() accessors
│   ├── tuner_ui_interface.h        TunerGUIInterface struct
│   ├── tuner_standby_ui_interface.h TunerStandbyGUIInterface struct
│   ├── tuner_math.h                TunerNoteName, name_for_note(), FrequencyInfo
│   └── defines.h                   Public subset of firmware constants
│
├── cmake/
│   └── qtune_plugin.cmake          qtune_project_so() — the C++ .so builder
│
├── docs/
│   ├── ALLOWED_SYMBOLS.md          Generated list of exported host symbols (§7)
│   ├── MONITOR.md                  Watching the serial console to debug plugins
│   └── FAQ.md                      Common questions (licensing, API boundaries, Windows)
│
├── tools/
│   └── validate_plugin.py          Offline .so checker (§9)
│
└── examples/
    ├── example_tuner/              Complete IDF project → example_tuner.so
    │   ├── CMakeLists.txt
    │   ├── sdkconfig.defaults
    │   ├── README.md
    │   └── main/
    │       ├── example_tuner.cpp   Implements TunerGUIInterface
    │       ├── CMakeLists.txt
    │       └── idf_component.yml
    │
    └── example_standby/            Complete IDF project → example_standby.so
        ├── CMakeLists.txt
        ├── sdkconfig.defaults
        ├── README.md
        └── main/
            ├── example_standby.cpp Implements TunerStandbyGUIInterface
            ├── CMakeLists.txt
            └── idf_component.yml
```

> When you copy an example out of this repo to start your own plugin, edit the
> single `QTUNE_SDK_DIR` line at the top of its `CMakeLists.txt` to point at your
> `q-tune-sdk` checkout.

---

## 4. The descriptor contract

Every plugin `.so` exports two things, both with C linkage (C++ name mangling
would hide them) and `QTUNE_PLUGIN_EXPORT` (default ELF visibility so they
survive `--strip-all`):

1. The **descriptor** data object `qtune_plugin_descriptor` — read statically by
   the SDK validator to check versions.
2. The **entry function** `qtune_plugin_entry()`, returning a pointer to that
   descriptor. The firmware calls this to obtain the descriptor at load time.

> Why a function *and* a data object? The ESP32 ELF loader's `dlsym()` resolves
> only **function** symbols in a loaded module, never data objects — so the
> firmware cannot read the descriptor data symbol directly. Exporting the small
> entry function is what makes the descriptor reachable. (Omitting it is the
> classic *"missing symbol 'qtune_plugin_entry'"* load failure.)

```cpp
// In your .cpp file.
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,   // must match firmware
    .lvgl_version = QTUNE_LVGL_VERSION,          // derived from LVGL headers
    .type         = QTUNE_PLUGIN_TUNER,          // or QTUNE_PLUGIN_STANDBY
    .sdk_build    = "my-plugin-1.0",             // freeform; logged only
    .interface    = &my_interface,               // -> TunerGUIInterface or
                                                 //    TunerStandbyGUIInterface
    .uid          = "qtune.my-tuner.k7f2q9",     // STABLE identity (scaffold
                                                 //   auto-generates; never change)
};

// Required entry point — the loader resolves this function, not the data above.
QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}
```

`uid` is the plugin's **permanent identity** — the firmware persists the user's UI
selection (and tracks crashes) by this string, not by a number. It is namespaced
and never a bare integer, the scaffold auto-generates one, and you must never
change it once published (see §6). The interface itself no longer has a `get_id()`
— the firmware assigns each plugin a numeric slot dynamically at load time.

The loader checks:

1. ABI `major.minor` (currently 1.0): the plugin's **major must equal** the
   firmware's and its **minor must be `<=`** the firmware's. Otherwise the plugin
   is skipped with an error log. See §5 for what bumps each component.
2. `QTUNE_LVGL_VERSION_COMPAT(lvgl_version) == QTUNE_LVGL_VERSION_COMPAT(firmware_lvgl_version)`.
   This compares only major.minor (top 16 bits). Mismatch → plugin skipped.
   Patch version drift (e.g. 9.2.0 vs 9.2.2) is allowed.

---

## 5. ABI and LVGL version rules

See [`COMPATIBILITY.md`](COMPATIBILITY.md) for the matrix of which SDK release,
ABI version, and LVGL pin go with which firmware version.

**Plugin ABI version (`major.minor`).** Your plugin's descriptor records the ABI
it was built against (`QTUNE_PLUGIN_ABI_MAJOR` / `QTUNE_PLUGIN_ABI_MINOR`). The
loader accepts it only when its **major equals** the firmware's and its **minor
is `<=`** the firmware's. A *breaking* change to the descriptor / interface
structs / exported-symbol contract bumps the major (older plugins are rejected);
a *backward-compatible* addition bumps the minor (older plugins keep loading; a
plugin needing a newer minor is rejected on older firmware). There is no patch
component — a change that doesn't alter the binary contract isn't an ABI change
and is tracked by the firmware version instead.

The `.so` is compiled against the same LVGL headers as the firmware. Every
LVGL struct (e.g. `lv_obj_t`, `lv_color_t`, `lv_style_t`) has its layout
baked into the binary at compile time. If the plugin and firmware use different
LVGL versions, or different `CONFIG_LV_COLOR_DEPTH` settings, the memory
layouts diverge and the plugin will corrupt memory or crash immediately.

**Rules:**

- Pin `lvgl/lvgl` to `"==9.2.2"` in `idf_component.yml`.
- Set `CONFIG_LV_COLOR_DEPTH=16` in `sdkconfig.defaults` — the firmware uses
  16-bit RGB565 and this affects `lv_color_t` size.
- Do not override `CONFIG_LV_USE_OS` — the firmware uses `LV_OS_FREERTOS`
  (value 2), which changes the lock field inside `lv_display_t`.
- If you add a widget type not enabled in the firmware, the linker will leave
  it as an undefined reference (resolved at load time). This is fine as long
  as the firmware exports that widget's symbols. Consult the firmware release
  notes for the list of exported LVGL widget families.

---

## 6. Plugin identity (the `uid`) and the numeric-ID pool

**You do not pick a number.** Each plugin carries a stable string `uid` in its
descriptor (e.g. `qtune.my-tuner.k7f2q9`) — that is its identity. The scaffolding
tool (`tools/new_plugin.py`) auto-generates one for you; advanced users can
override it with `--uid`.

- **Stable.** Once your plugin is deployed and users have selected it, **never
  change the uid.** The firmware stores the selected UI by uid in NVS
  (non-volatile storage); changing it silently selects a different UI on next
  boot. Copying a plugin to make a new one needs a *fresh* uid (re-scaffold; the
  tool generates a new one).
- **Unique & namespaced.** A uid is namespaced so two unrelated plugins never
  collide without any central registry, and it must **never be a bare integer**
  — that space is reserved for built-in UIs, whose uid is simply their fixed
  number as a string (`"0"`, `"1"`, …). The loader rejects a plugin whose uid is
  a bare integer or duplicates another plugin's uid.

The numeric ID is now a **firmware-internal assignment pool**, not something an
author chooses. At load time the firmware hands each plugin the next free number
in its reserved range (skipping built-ins and already-loaded plugins); a plugin's
number may even differ between boots, which is fine because nothing persistent
references it. For reference, the reserved pools are:

| Plugin type | Firmware assignment pool |
|-------------|--------------------------|
| Tuner (`TunerGUIInterface`) | **100 – 199** |
| Standby (`TunerStandbyGUIInterface`) | **210 – 254** |

---

## 7. Allowed API surface

### LVGL widgets and functions

The firmware exports an explicit allowlist of LVGL symbols. Calling any `lv_*`
function not in that list causes the loader to fail to resolve the relocation
and the plugin will not load.

**The complete, authoritative list is in
[`docs/ALLOWED_SYMBOLS.md`](docs/ALLOWED_SYMBOLS.md)** — it is generated from the
firmware, so it never drifts. The offline validator (§9) flags any unexported
`lv_*` call before you upload. A few high-value gotchas:

- Use `lv_scale_*` (fully exported) rather than `lv_arc_set_value` /
  `lv_arc_set_range` (not exported) for needle/gauge indicators.
- `lv_color_make` is not exported — use `lv_color_hex(0xRRGGBB)`.
- LVGL 8 compat macros (`lv_img_*`, `lv_obj_clear_flag`, `lv_timer_del`) have no
  ELF symbols; use the LVGL 9 names (`lv_image_*`, `lv_obj_remove_flag`,
  `lv_timer_delete`).


### Q-Tune host API (`qtune_plugin_host_api.h`)

| Symbol | Type | Description |
|--------|------|-------------|
| `screen_width` | `lv_coord_t` (global) | Current display width in pixels |
| `screen_height` | `lv_coord_t` (global) | Current display height in pixels |
| `is_landscape` | `bool` (global) | True when width > height |
| `qt_get_reference_frequency()` | `int32_t` | A4 reference pitch in Hz (default 440) |
| `qt_get_in_tune_cents_width()` | `uint8_t` | Half-width of in-tune window in cents (default 3) |
| `qt_get_monitoring_mode()` | `uint8_t` | Non-zero when monitoring mode is active |
| `qt_get_note_name_palette()` | `lv_palette_t` | User's accent colour (`LV_PALETTE_NONE` = amber retro) |
| `qt_get_show_cents()` | `uint8_t` | Non-zero when the user wants the cents value shown |
| `qt_get_note_glyph(note, size)` | `const lv_image_dsc_t *` | Built-in note letter artwork (A–G / blank); pair with `qt_get_sharp_glyph()` |
| `qt_get_sharp_glyph(size)` / `qt_get_blank_glyph(size)` | `const lv_image_dsc_t *` | Sharp (#) overlay / blank glyph. Sizes: `QT_GLYPH_SIZE_SMALL`…`_XLARGE` |
| `qt_get_mute_glyph()` | `const lv_image_dsc_t *` | The mute indicator icon (draw when `show_mute_indicator` is set) |
| `qt_note_is_sharp(note)` | `uint8_t` | Non-zero for A#, C#, D#, F#, G# |
| `qt_uptime_ms()` | `uint32_t` | Monotonic milliseconds since boot — for animation / elapsed-time |
| `qt_random_u32()` | `uint32_t` | Hardware random number — for particles, jitter, quiz prompts |

**Touch input.** The pedal has a touchscreen. React to taps by adding an event
callback: `lv_obj_add_event_cb(obj, cb, LV_EVENT_PRESSED, NULL)`, then inside the
callback read the point with `lv_event_get_indev(e)` + `lv_indev_get_point(...)`.
See [`docs/TOUCH.md`](docs/TOUCH.md).

### Standard library

`libc` functions (`printf`, `snprintf`, `memcpy`, `memset`, `malloc`, `free`,
`strncpy`, etc.) and `<math.h>` functions (`fabsf`, `log2f`, `powf`, `roundf`,
etc.) are available.

### What is NOT available

- Direct hardware register access or GPIO control.
- NVS (non-volatile storage) read/write.
- Wi-Fi or network sockets.
- `esp_log_*` functions (use `printf` for debug output — it goes to the serial
  monitor).
- FreeRTOS task creation (`xTaskCreate`). Do not create new RTOS tasks from
  plugin code; use `lv_timer_create` for periodic work instead.
- `esp_lvgl_port_lock` / `esp_lvgl_port_unlock`. The firmware holds the LVGL
  lock during all plugin callbacks. Calling the lock functions from inside a
  callback will deadlock.
- Any internal firmware symbol not listed in the export table above.

---

## 8. Plugin lifecycle

### Tuner plugin (`TunerGUIInterface`)

```
firmware boots
    │
    ├── init(screen)          ← called once when tuning mode is entered.
    │                           Create all LVGL objects here. Save `screen`.
    │
    │   [tuning mode active — ~30 fps]
    ├── display_frequency(...)  ← called each GUI frame with latest pitch data.
    │                             Update labels, arcs, needle positions, etc.
    │                             Keep this function fast (< 1 ms).
    │
    ├── align_settings_button(btn)  ← called once after init. Position the
    │                                  gear button (e.g. bottom-right corner).
    │
    └── cleanup()             ← called when tuning mode exits or the user picks
                                 a different tuner. Delete timers/animations
                                 here. Do NOT delete LVGL children of `screen`
                                 — the host deletes them automatically.
```

### Standby plugin (`TunerStandbyGUIInterface`)

```
firmware enters standby
    │
    ├── enable_screen()       ← return true to keep display on, false to blank.
    │
    ├── init(screen)          ← create objects, start lv_timers/animations.
    │
    │   [standby active — ~50 fps timer, ~30 fps display_frequency]
    ├── display_frequency(...)  ← optional pitch-reactive updates.
    │
    └── cleanup()             ← stop lv_timers, null pointers, return.
```

### Key rules

- **Do not call `lvgl_port_lock` / `lvgl_port_unlock`** from any plugin
  function. The host holds the LVGL mutex for the entire duration of `init`,
  `cleanup`, `display_frequency`, and any `lv_timer` callback.
- **Delete your timers in `cleanup()`** before the host clears the screen.
  A dangling `lv_timer` pointing to freed objects will crash the firmware on
  its next tick.
- **Do not call `lv_obj_del()` on children of `screen`** in `cleanup()`. The
  host deletes all screen children after calling `cleanup()`. Only delete
  objects you created as children of objects *other* than `screen` (rare).
- `display_frequency` is called at approximately 30 Hz. Cache the previous
  value and skip the LVGL update when the value has not changed significantly
  (see the examples for the pattern).
- `display_frequency` receives `NOTE_NONE` as `note_name` when no pitch is
  detected. Always handle this case gracefully.
- **Design for both orientations.** The pedal can be used in **portrait
  (240×320)** or **landscape (320×240)**, and the user can switch between them.
  Lay out your UI relative to the live `screen_width` / `screen_height` /
  `is_landscape` globals rather than hard-coding `240`/`320`, so it looks right
  either way. Both examples show this: `example_tuner` chooses a portrait vs.
  landscape arrangement in `init()`, and `example_standby` recomputes its bounds
  from the live globals on every frame. Your `init()` sees whichever orientation
  is active when the UI starts; if you also want to adapt while it's running (the
  user can rotate the display), read the globals during `display_frequency` / your
  `lv_timer` and reposition, the way `example_standby` does.
- **Consider smoothing the readings (tuner — optional, to taste).** Pitch data
  arrives ~30×/second and can be a little jittery. Easing your needle/indicator
  toward the new value, or low-pass-filtering the cents, makes the display feel
  calm instead of twitchy. This is a stylistic choice, **not** a requirement —
  some designs (strobes, fast meters) deliberately show the raw, immediate
  movement. Do whatever fits the look you want.

---

## 9. Build → upload → activate

### Build

With Docker (recommended — see §2):

```sh
./docker-build.sh            # from inside the plugin project dir
```

Or with a native ESP-IDF install (after sourcing `export.sh`):

```sh
idf.py set-target esp32s3    # first time only
idf.py build                 # downloads components on first run, then links the .so
```

Either way, the `.so` is written to `build/<project-name>.so`.

If the build fails with a CMakeCache path error (stale cache after moving the
project directory):

```sh
rm build/CMakeCache.txt build/bootloader/CMakeCache.txt
idf.py build
```

### Validate (recommended, before uploading)

Catch problems on your host instead of at boot. The validator runs the same
checks as the firmware loader (descriptor present, ABI/LVGL match, no unexported
`lv_*` calls):

```sh
python3 /path/to/q-tune-sdk/tools/validate_plugin.py build/<project-name>.so
```

The example projects also expose it as a CMake target:

```sh
cmake --build build --target validate
```

### Upload

#### Using the Built-in Wi-Fi Server
1. On Q-Tune, navigate to Settings > Wi-Fi and connect to your network. The
   device's IP address is shown on that screen.
2. In a browser, open `http://<device-ip>/plugins`.
3. Use the upload form to select your `.so` file.
4. The file is written to `/data/plugins/` on the device's internal flash.

#### Using USB Drive Mode
1. With Q-Tune powered off, press and hold the foot switch while connecting it to your computer with a USB-C cable.
2. Upload your `.so` file to the `/plugins/` folder. NOTE: If you've never uploaded a plugin, you may need to create the `/plugins/` directory at the root of the Q-Tune file system.
3. Properly eject Q-Tune as a USB Drive.
4. Disconnect the USB-C cable.

### Activate

Restart the device (power-cycle or Settings > Restart). At boot the firmware
scans `/data/plugins/`, loads each `.so` via `elf_loader`, and registers
valid plugins. Your plugin then appears in:

- Settings > Tuner UI — for `QTUNE_PLUGIN_TUNER` plugins.
- Settings > Standby Screen — for `QTUNE_PLUGIN_STANDBY` plugins.

Select it from the appropriate menu. The selection is persisted to NVS by
the firmware.

### Debugging

The easiest way to observe the console output (debug messages) of Q-Tune is to use the web installer tool:

1. Plug Q-Tune with a USB-C cable to your computer.
2. Use the Chrome web browser and navigate to the [Firmware Installer](https://www.q-tune.com/install/) page.
3. Scroll down and press the **Connect to device** button.
4. Select Q-Tune from the list of devices (usually shows "USB JTAG...") and press the **Connect** button.
5. Click the **Logs & Console** option.
6. Q-Tune system and log messages will appear in the browser window.

---

## 10. Crash recovery and safe mode

### Per-plugin crash tracking

If a plugin causes a hard fault or watchdog timeout during `init` or
`display_frequency`, the firmware detects the reset reason and increments a
crash counter in NVS for that plugin. After **2 consecutive crashes** the
firmware marks the plugin as disabled and falls back to the default built-in
UI (Meter for tuner, Basic for standby). A log message identifying the plugin
and crash count is written to the serial monitor.

Re-enable a disabled plugin from the `/plugins` web page.

### Safe mode (BOOT button)

Hold the **BOOT button (GPIO0)** during power-on to enter Safe Mode. In Safe
Mode all plugins are bypassed and the firmware runs only built-in UIs. This
lets you recover from a plugin that crashes during boot before the Wi-Fi stack
is up.

While in Safe Mode, connect over Wi-Fi (the firmware still starts the server)
and visit `/plugins` to delete or update the offending plugin. Normal plugin
loading resumes on the next restart without the BOOT button held.

### Plugin management page (`/plugins`)

The `/plugins` web page lets you:

- View all installed plugins with their uid, type, and enabled/disabled status.
- Upload a new `.so` (replaces an existing file of the same name).
- Delete an installed plugin.
- Re-enable a crash-disabled plugin.

---

## 11. Quick-start checklist

Copy one of the example projects to a new directory and follow these steps:

- [ ] Leave the descriptor `.uid` as the scaffold generated it (your stable
      identity — never change it after publishing). You don't pick a number; the
      firmware assigns one dynamically at load.
- [ ] Update `get_name()` to a short descriptive string (shown in the menu).
- [ ] Set `sdk_build` in the descriptor to a version string for your plugin.
- [ ] Update `idf_component.yml`: keep `lvgl/lvgl: "==9.2.2"`.
- [ ] Update `sdkconfig.defaults`: keep `CONFIG_LV_COLOR_DEPTH=16` and
      `CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=y`.
- [ ] Update `CMakeLists.txt` `project()` name and `qtune_project_so()` name to
      match your plugin filename.
- [ ] Implement `init()`, `display_frequency()`, `cleanup()` (and
      `align_settings_button()` for tuner plugins).
- [ ] Delete any `lv_timer` / `lv_anim` you create in `cleanup()`.
- [ ] Build with `idf.py build`, upload to `/plugins`, restart, select in UI.

---

## 12. Security

A plugin is **native compiled code that runs unsandboxed** inside the firmware.
There is no privilege boundary between a plugin and the rest of the device: a
plugin runs with the same access as the built-in UIs and can read the host API,
allocate memory, and draw to the screen directly.

**Only install plugins you trust — ideally only ones you built yourself**, or that
come from a source you trust, with source code you (or Claude Code) can read.
Treat a `.so` from an unknown third party the way you would any untrusted program:
don't run it on hardware you care about without reviewing it first.

The firmware's crash-quarantine (after 2 crashes a plugin is disabled — see §10)
and Safe Mode are **stability safety nets, not a security boundary.** They protect
you from a buggy plugin, not a malicious one. When you build a plugin with the SDK
from source, you can see exactly what it does — which is the safest path, and the
one this SDK is designed around.

---

## 13. Disclaimer

This SDK and any plugins built with it are provided **"as is", without warranty of
any kind.** You build, install, and run plugins **entirely at your own risk** — a
faulty plugin can crash the pedal or render it temporarily unusable. To the maximum
extent permitted by law, neither **Boyd Timothy** nor **Molinello Music** is liable
for any damage to your device, equipment, or data, or any other loss arising from
the use of this SDK or any plugin built with it. **Only install plugins you trust.**

See [`DISCLAIMER.md`](DISCLAIMER.md) for the full text and the Apache-2.0
[`LICENSE`](LICENSE) (Sections 7 and 8) for the governing warranty disclaimer and
limitation of liability.
