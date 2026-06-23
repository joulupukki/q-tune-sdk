# q-tune-sdk

The complete end-user SDK for writing UI plugins for the Q-Tune ESP32-S3 guitar
tuner pedal. A plugin is a small C/C++ module compiled to a relocatable shared
object (`.so`). The firmware loads it from `/data/plugins/` on internal flash at
boot and registers it alongside the built-in UIs.

Licensed under **Apache-2.0** (see [`LICENSE`](LICENSE)) — you are free to
develop and **sell** plugins built with this SDK.

---

## Table of Contents

1. [Plugin model overview](#1-plugin-model-overview)
2. [Prerequisites](#2-prerequisites)
3. [SDK directory layout](#3-sdk-directory-layout)
4. [The descriptor contract](#4-the-descriptor-contract)
5. [ABI and LVGL version rules](#5-abi-and-lvgl-version-rules)
6. [Reserved ID ranges](#6-reserved-id-ranges)
7. [Allowed API surface](#7-allowed-api-surface)
8. [Plugin lifecycle](#8-plugin-lifecycle)
9. [Build → upload → activate](#9-build--upload--activate)
10. [Crash recovery and safe mode](#10-crash-recovery-and-safe-mode)
11. [Quick-start checklist](#11-quick-start-checklist)

---

## 1. Plugin model overview

```
  Plugin project (your code)
  ───────────────────────────
  example_tuner.cpp
      implements TunerGUIInterface
      exports   qtune_plugin_descriptor
        │
        │  idf.py build
        ▼
  example_tuner.so   ← position-independent, all internals hidden
        │
        │  upload via http://<device-ip>/plugins
        ▼
  /data/plugins/example_tuner.so   (internal flash, FAT32)
        │
        │  at boot: espressif/elf_loader reads .so,
        │           resolves undefined symbols against
        │           the firmware's export table,
        │           dlsym()s qtune_plugin_descriptor,
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

Requires only [Docker](https://docs.docker.com/get-docker/). The pinned
`espressif/idf:v5.3.2` image carries the whole toolchain.

```sh
# Build any plugin project (writes build/<name>.so):
./docker-build.sh examples/example_tuner

# …or from inside a project directory:
cd examples/example_tuner && ../../docker-build.sh
```

For interactive development, open the repo in **VS Code → "Reopen in
Container"** (the `.devcontainer/` config builds the same pinned image and puts
`idf.py` on your `PATH`).

### Option B — native ESP-IDF install

| Tool / component | Version |
|------------------|---------|
| ESP-IDF          | v5.3.2 (exactly — the firmware is built with this) |
| IDF target       | `esp32s3` |
| Python           | 3.8 or newer |
| Host OS          | macOS or Linux (Windows: use WSL2 or Docker) |

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
It needs Python 3 and `pyelftools`:

```sh
pip install pyelftools
```

---

## 3. SDK directory layout

```
q-tune-sdk/
├── README.md                       This guide
├── LICENSE  /  NOTICE              Apache-2.0 (you may sell your plugins)
├── COMPATIBILITY.md                SDK ↔ ABI ↔ LVGL ↔ firmware version matrix
├── Dockerfile  /  docker-build.sh  Pinned ESP-IDF v5.3.2 build environment
├── .devcontainer/                  VS Code "Reopen in Container" config
│
├── include/                        SDK headers — add this dir to your include path
│   ├── qtune_plugin.h              Umbrella header (include only this)
│   ├── qtune_plugin_abi.h          QTunePluginDescriptor, QTUNE_PLUGIN_EXPORT, ID ranges
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
│   └── ALLOWED_SYMBOLS.md          Generated list of exported host symbols (§7)
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

Every plugin `.so` must export **exactly one** symbol with C linkage named
`qtune_plugin_descriptor` (the value of `QTUNE_PLUGIN_DESCRIPTOR_SYMBOL`).
Declare it with `QTUNE_PLUGIN_EXPORT` to give it default ELF visibility so it
survives `--strip-all` and appears in `.dynsym` where `dlsym()` can find it.

```cpp
// In your .cpp file — C linkage is required; C++ name mangling would hide it.
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,   // must match firmware
    .lvgl_version = QTUNE_LVGL_VERSION,          // derived from LVGL headers
    .type         = QTUNE_PLUGIN_TUNER,          // or QTUNE_PLUGIN_STANDBY
    .sdk_build    = "my-plugin-1.0",             // freeform; logged only
    .interface    = &my_interface,               // -> TunerGUIInterface or
                                                 //    TunerStandbyGUIInterface
};
}
```

The loader checks:

1. `abi_version == QTUNE_PLUGIN_ABI_VERSION` (currently 1). Mismatch → plugin
   is skipped with an error log.
2. `QTUNE_LVGL_VERSION_COMPAT(lvgl_version) == QTUNE_LVGL_VERSION_COMPAT(firmware_lvgl_version)`.
   This compares only major.minor (top 16 bits). Mismatch → plugin skipped.
   Patch version drift (e.g. 9.2.0 vs 9.2.2) is allowed.

---

## 5. ABI and LVGL version rules

See [`COMPATIBILITY.md`](COMPATIBILITY.md) for the matrix of which SDK release,
ABI version, and LVGL pin go with which firmware version.

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

## 6. Reserved ID ranges

Plugin IDs must not collide with built-in firmware IDs or other plugins. The
firmware tries each registered ID in sorted order; duplicate IDs are silently
skipped (the later-loaded plugin wins for that ID slot).

| Plugin type | Reserved range | Built-in IDs to avoid |
|-------------|---------------|----------------------|
| Tuner (`TunerGUIInterface`) | **100 – 199** | 0–9, 200–255 |
| Standby (`TunerStandbyGUIInterface`) | **210 – 254** | 0–5, 200–201 |

Pick **any unused value** in your range. If two plugins share an ID, the
firmware logs a warning and only one will appear in the menu.

Once your plugin is deployed and users have selected it, **never change its
ID**. The firmware stores the selected UI by ID in NVS (non-volatile storage);
an ID change silently selects a different UI on next boot.

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

1. On Q-Tune, navigate to Settings > Wi-Fi and connect to your network. The
   device's IP address is shown on that screen.
2. In a browser, open `http://<device-ip>/plugins`.
3. Use the upload form to select your `.so` file.
4. The file is written to `/data/plugins/` on the device's internal flash.

### Activate

Restart the device (power-cycle or Settings > Restart). At boot the firmware
scans `/data/plugins/`, loads each `.so` via `elf_loader`, and registers
valid plugins. Your plugin then appears in:

- Settings > Tuner UI — for `QTUNE_PLUGIN_TUNER` plugins.
- Settings > Standby Screen — for `QTUNE_PLUGIN_STANDBY` plugins.

Select it from the appropriate menu. The selection is persisted to NVS by
the firmware.

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

- View all installed plugins with their ID, type, and enabled/disabled status.
- Upload a new `.so` (replaces an existing file of the same name).
- Delete an installed plugin.
- Re-enable a crash-disabled plugin.

---

## 11. Quick-start checklist

Copy one of the example projects to a new directory and follow these steps:

- [ ] Update `get_id()` to an unused ID in the correct range.
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
