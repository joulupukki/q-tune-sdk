# example_standby — Q-Tune Standby Plugin Example

This project builds `example_standby.so`, a minimal standby UI plugin for
Q-Tune. It demonstrates how to implement `TunerStandbyGUIInterface`, manage
an `lv_timer` for animation, and use `display_frequency()` to react to
detected pitch.

## What it looks like

A small white dot bounces around the screen at a constant speed. When the
device is in Buffered Bypass mode and the user plays a note, the dot turns
the user's configured accent colour. When the note stops, it returns to white.

## Prerequisites

See the [SDK README §2](../../README.md#2-prerequisites) for the full toolchain
setup. In short: either Docker (recommended) or a native ESP-IDF v5.3.2 install
targeting `esp32s3`.

## Build steps

With Docker (no local ESP-IDF needed):

```sh
../../docker-build.sh        # run from this directory
```

Or with a native ESP-IDF install (after `. <idf>/export.sh`):

```sh
idf.py set-target esp32s3
idf.py build
```

Output: `build/example_standby.so`

Validate it before uploading (optional but recommended):

```sh
cmake --build build --target validate
```

## Upload

1. Connect Q-Tune to Wi-Fi (Settings > Wi-Fi).
2. Open `http://<device-ip>/plugins` in a browser.
3. Upload `build/example_standby.so`.
4. Restart the device.
5. Select "Dot" from Settings > Standby Screen.

## Plugin ID

`get_id()` returns **210** — in the reserved standby plugin range [210, 254].
Change it only if there is a collision with another installed plugin.

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
  line in `es_display_frequency()`.
- To make the dot speed proportional to the detected octave (like the built-in
  Bouncy Ball), map the `octave` parameter to `s_dx` / `s_dy` inside
  `es_display_frequency()`.
- To add a trail effect, create several smaller dots and shift their positions
  one behind the lead dot on each timer tick.
