# bouncer — Q-Tune Standby Plugin Sample

This project builds `bouncer.so`, a minimal standby UI plugin for Q-Tune. It's
one of the bundled standby samples (alongside [`hyperdrive`](../hyperdrive)) — a
clean starting point to copy and build on. It demonstrates how to implement
`TunerStandbyGUIInterface`, manage an `lv_timer` for animation, react to touch,
and use `display_frequency()` to react to detected pitch.

## What it looks like

A small white dot bounces around the screen at a constant speed. When the
device is in Buffered Bypass mode and the user plays a note, the dot turns
the user's configured accent colour. When the note stops, it returns to white.

**Tap the screen** and the dot jumps to your finger and sets off in a new random
direction — a small demonstration of touchscreen input (`lv_obj_add_event_cb` +
`lv_indev_get_point`) and the `qt_random_u32()` host accessor. See
[`docs/TOUCH.md`](../../../docs/TOUCH.md) for the full touch pattern.

## Prerequisites

See the [SDK reference, §2 Prerequisites](../../../docs/REFERENCE.md#2-prerequisites) for the full toolchain
setup. In short: either Docker (recommended) or a native ESP-IDF v5.3.2 install
targeting `esp32s3`.

## Build steps

With Docker (no local ESP-IDF needed) — from the SDK repo root:

```sh
./docker-build.sh plugins/standby/bouncer
```

Or with a native ESP-IDF install (after `. <idf>/export.sh`), from this directory:

```sh
idf.py set-target esp32s3
idf.py build
```

Output: `build/bouncer.so`

The Docker build runs the validator automatically. To validate a native build:

```sh
cmake --build build --target validate
```

## Upload

1. Put Q-Tune in USB Drive Mode (hold the foot switch at power-on) and copy
   `build/bouncer.so` into the `/plugins` folder. Or upload it over Wi-Fi at
   `http://<device-ip>/plugins`. See [`docs/DEPLOY.md`](../../../docs/DEPLOY.md).
2. Restart the device.
3. Select "Bouncer" from Settings > Standby Screen.

## Plugin identity

The descriptor's `uid` is `"qtune.bouncer.0001"` — the plugin's stable identity.
The firmware assigns the numeric menu slot dynamically at load; you don't pick a
number. The scaffolding tool (`tools/new_plugin.py`) generates a unique `uid`
for your own plugin. Never change a `uid` after publishing.

## Important: timer lifecycle

The plugin creates an `lv_timer` in `init()` and **must** delete it in
`cleanup()` via `lv_timer_delete()`. Forgetting to do so leaves a timer
firing against freed LVGL objects and will crash the firmware.

Do NOT call `lvgl_port_lock()` / `lvgl_port_unlock()` anywhere in plugin
code. The firmware holds the LVGL lock during all plugin callbacks
(`init`, `cleanup`, `display_frequency`) and the timer callback fires
inside `lv_timer_handler()`, which also runs under the lock.

## Customising

- To change the dot colour when not in tune, edit the `colour = lv_color_white()`
  line in `bo_display_frequency()`.
- To make the dot speed proportional to the detected octave, map the `octave`
  parameter to `s_dx` / `s_dy` inside `bo_display_frequency()`.
- To add a trail effect, create several smaller dots and shift their positions
  one behind the lead dot on each timer tick.
