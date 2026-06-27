# gauge — Q-Tune Tuner Plugin Sample

This project builds `gauge.so`, a minimal tuner UI plugin for Q-Tune. It's one of
the bundled tuner samples (alongside [`phase`](../phase)) — a clean starting
point to copy and build on. It demonstrates how to implement `TunerGUIInterface`,
export the plugin descriptor, and use LVGL widgets (note glyphs + an `lv_scale`
needle gauge) together with the host API accessors (`qt_get_note_glyph`,
`qt_get_mute_glyph`, `qt_get_in_tune_cents_width`, `qt_get_note_name_palette`,
etc.).

## What it looks like

- The detected note drawn with the firmware's built-in artwork: a letter glyph
  (A–G) plus a sharp (#) overlay for accidentals.
- A round `lv_scale` needle gauge whose needle sweeps with the cents deviation
  as the detected pitch drifts flat or sharp.
- The note and needle switch to the user's accent colour when the pitch is
  within the configured in-tune window.
- A **mute indicator** in the top-left corner, shown only while the signal is
  muted, in the user's selected note-name accent colour
  (`qt_get_note_name_palette()`).
- The **Q-Tune logo settings button** (provided by the firmware) in the
  bottom-right corner — tap it to open settings.

It deliberately does **not** draw a reference-pitch readout: the firmware shows
its own reference-pitch indicator (bottom-centre) when the tuner loads and when
you change it.

The layout is orientation-aware (portrait: note on top, gauge on the bottom;
landscape: note on the left, gauge on the right).

## Prerequisites

See the [SDK reference, §2 Prerequisites](../../../docs/REFERENCE.md#2-prerequisites) for the full toolchain
setup. In short: either Docker (recommended) or a native ESP-IDF v5.3.2 install
targeting `esp32s3`.

## Build steps

With Docker (no local ESP-IDF needed) — from the SDK repo root:

```sh
./docker-build.sh plugins/tuner/gauge
```

> The first build pulls the pinned ESP-IDF Docker image (a few GB) and the LVGL /
> elf_loader components — expect several minutes. Later builds reuse the cache and
> finish in seconds.

Or with a native ESP-IDF install (after `. <idf>/export.sh`), from this directory:

```sh
idf.py set-target esp32s3    # only needed once; written into sdkconfig
idf.py build                 # downloads lvgl 9.2.2 + elf_loader on first run
```

The shared object is written to:

```
build/gauge.so
```

The Docker build runs the validator automatically. To validate a native build:

```sh
cmake --build build --target validate
```

If the build fails with a CMakeCache path error (e.g. after moving the project
directory), delete the stale cache and rebuild:

```sh
rm -rf build && idf.py build
```

## Upload

1. Put Q-Tune in USB Drive Mode (hold the foot switch at power-on) and copy
   `build/gauge.so` into the `/plugins` folder. Or upload it over Wi-Fi at
   `http://<device-ip>/plugins`. See [`docs/DEPLOY.md`](../../../docs/DEPLOY.md).
2. Restart the device. The firmware loads plugins from `/data/plugins/` at boot.
3. Select "Gauge" from Settings > Tuner > Style on the device.
4. No guitar handy? Enable *Settings > Advanced > Developer Tools > Test Signal*
   to drive the needle with a built-in sweep through the six guitar strings (each
   easing from off-pitch into tune). Turn it off again for live tuning.

## Plugin identity

The descriptor's `uid` is `"qtune.gauge.0001"` — the plugin's stable identity.
The firmware assigns the numeric menu slot dynamically at load; you don't pick a
number. Your own plugin gets a unique `uid` automatically from the scaffolding
tool (`tools/new_plugin.py`). Never change a `uid` after publishing or the user's
saved selection of that UI is lost.

## Customising

- To change the gauge sweep or orientation, edit the `lv_scale_set_angle_range()`
  / `lv_scale_set_rotation()` calls in `ga_init()`.
- To add a cents readout label, create an `lv_label` and update it in
  `ga_display_frequency()` when `qt_get_show_cents()` returns non-zero.
- To tint the in-tune state differently, change the colour logic in
  `ga_display_frequency()` (it reads `qt_get_note_name_palette()`).
