# example_tuner — Q-Tune Tuner Plugin Example

This project builds `example_tuner.so`, a minimal tuner UI plugin for Q-Tune.
It demonstrates how to implement `TunerGUIInterface`, export the plugin
descriptor, and use LVGL widgets (note glyphs + an `lv_scale` needle gauge)
together with the host API accessors (`qt_get_note_glyph`, `qt_get_mute_glyph`,
`qt_get_in_tune_cents_width`, `qt_get_note_name_palette`, etc.).

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
idf.py set-target esp32s3    # only needed once; written into sdkconfig
idf.py build                 # downloads lvgl 9.2.2 + elf_loader on first run
```

The shared object is written to:

```
build/example_tuner.so
```

Validate it before uploading (optional but recommended):

```sh
cmake --build build --target validate
```

If the build fails with a CMakeCache path error (e.g. after moving the project
directory), delete the stale cache and rebuild:

```sh
rm build/CMakeCache.txt build/bootloader/CMakeCache.txt
idf.py build
```

## Upload

1. Connect Q-Tune to your Wi-Fi (Settings > Wi-Fi on the device).
2. Open a browser and navigate to `http://<device-ip>/plugins`
   (the IP address is shown on the Wi-Fi settings screen).
3. Use the upload form to select `build/example_tuner.so`.
4. After upload, restart the device. The firmware loads plugins from
   `/data/plugins/` at boot.
5. Select "Example" from Settings > Tuner UI on the device.

## Plugin identity

The descriptor's `uid` is `"qtune.example-tuner.0001"` — the plugin's stable
identity. The firmware assigns the numeric menu slot dynamically at load; you don't
pick a number. Your own plugin gets a unique `uid` automatically from the
scaffolding tool (`tools/new_plugin.py`). Never change a `uid` after publishing or
the user's saved selection of that UI is lost.

## Customising

- To change the gauge sweep or orientation, edit the `lv_scale_set_angle_range()`
  / `lv_scale_set_rotation()` calls in `et_init()`.
- To use a different font size, change `&lv_font_montserrat_48` in `et_init()`
  and enable the corresponding `CONFIG_LV_FONT_MONTSERRAT_*` in `sdkconfig.defaults`.
- To add a cents readout label, create an `lv_label` and update it in
  `et_display_frequency()` when `qt_get_show_cents()` returns non-zero.
