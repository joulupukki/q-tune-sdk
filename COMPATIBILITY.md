<!-- The current-row values are stamped by tools/sync_sdk.py at release time. -->

# Compatibility Matrix

A plugin's compiled `.so` is tied to the firmware it runs on by two hard
contracts the loader enforces at boot:

- **ABI version** (`major.minor`) — the loader requires the **same major** and a
  plugin **minor `<=`** the firmware's. Major bumps on a breaking change to
  `QTunePluginDescriptor`, the interface structs, or the exported-symbol
  contract; minor bumps on a backward-compatible addition (so a plugin built
  against an older minor keeps working). (`QTUNE_PLUGIN_ABI_MAJOR` /
  `QTUNE_PLUGIN_ABI_MINOR` in `include/qtune_plugin_abi.h`.)
- **LVGL major.minor** — must match. LVGL struct layouts are baked into the
  plugin at compile time, so the plugin and firmware must share the same
  `major.minor` (patch drift is allowed). Pin `lvgl/lvgl` accordingly in your
  `idf_component.yml`.

Build your plugin with the row that matches the firmware on your device.

| SDK release | ABI version | LVGL (pin) | ESP-IDF | Firmware |
|-------------|-------------|------------|---------|----------|
| 1.0.0 | 1.0 | 9.2.x (`==9.2.2`) | v5.3.2 | Q-Tune 5.1.0+ |

## How to check what your device runs

- **Firmware version:** Settings > About on the device.
- **A plugin's built-against versions:** run the offline validator, which prints
  the `.so`'s `abi_version` and `lvgl_version`:

  ```sh
  python3 tools/validate_plugin.py build/my_plugin.so
  ```

If the loader rejects a plugin, the reason (ABI mismatch / LVGL mismatch) is
shown on the device's `/plugins` web page and in the serial log.
