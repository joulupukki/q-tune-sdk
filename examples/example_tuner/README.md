# example_tuner — Q-Tune Tuner Plugin Example

This project builds `example_tuner.so`, a minimal tuner UI plugin for Q-Tune.
It demonstrates how to implement `TunerGUIInterface`, export the plugin
descriptor, and use LVGL widgets (label + `lv_scale` needle gauge) together with
the host API accessors (`qt_get_reference_frequency`,
`qt_get_in_tune_cents_width`, etc.).

## What it looks like

- A large note-name + octave label in the centre (e.g. "A4").
- A round `lv_scale` needle gauge whose needle sweeps with the cents deviation
  as the detected pitch drifts flat or sharp.
- The needle and note label switch to the user's accent colour when the
  pitch is within the configured in-tune window.
- A small "A4=440" reference-frequency label in the top-right corner.

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

## Plugin ID

`get_id()` returns **100**. This places the plugin in the reserved tuner
plugin range [100, 199]. Change this value only if it collides with another
plugin you have installed — IDs must be unique across all loaded plugins.

## Customising

- To change the gauge sweep or orientation, edit the `lv_scale_set_angle_range()`
  / `lv_scale_set_rotation()` calls in `et_init()`.
- To use a different font size, change `&lv_font_montserrat_48` in `et_init()`
  and enable the corresponding `CONFIG_LV_FONT_MONTSERRAT_*` in `sdkconfig.defaults`.
- To add a cents readout label, create an `lv_label` and update it in
  `et_display_frequency()` when `qt_get_show_cents()` returns non-zero.
