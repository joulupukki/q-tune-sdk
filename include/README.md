# SDK Headers

Include only `qtune_plugin.h` in your plugin source. It pulls in all of the
other headers in the correct order.

## Files

| File | Origin | Notes |
|------|--------|-------|
| `qtune_plugin.h` | SDK (umbrella) | Include this; includes everything else |
| `qtune_plugin_abi.h` | Firmware (verbatim) | Descriptor type, QTUNE_PLUGIN_EXPORT, ID ranges |
| `qtune_plugin_host_api.h` | Firmware (verbatim) | screen_width/height, qt_get_*() accessors |
| `tuner_ui_interface.h` | Firmware (verbatim) | TunerGUIInterface struct |
| `tuner_standby_ui_interface.h` | Firmware (verbatim) | TunerStandbyGUIInterface struct |
| `tuner_math.h` | Firmware (verbatim) | TunerNoteName, name_for_note(), FrequencyInfo |
| `defines.h` | Firmware (TRIMMED) | Public constants only — no GPIO, no task config |

The four verbatim files are copied directly from the firmware source and are
authoritative. If the firmware ships an updated ABI (bumped
`QTUNE_PLUGIN_ABI_VERSION`), download the new SDK and rebuild your plugin
before uploading it to updated firmware.

## LVGL headers and lv_conf.h

LVGL 9 resolves its configuration by reading `lv_conf.h` at compile time.
The correct approach for plugin projects is to let the IDF component manager
handle this: when `lvgl/lvgl` is listed as a component dependency, the
component's Kconfig system generates `lv_conf.h` from your `sdkconfig`.

**The firmware uses the following LVGL Kconfig settings.** Your plugin's
`sdkconfig` / `sdkconfig.defaults` MUST agree with every setting that affects
struct layout or type sizes. The most critical ones are:

```
CONFIG_LV_COLOR_DEPTH=16          # 16-bit RGB565 — affects lv_color_t size
CONFIG_LV_OS_FREERTOS=y           # affects lv_display_t internal lock field
CONFIG_LV_USE_OS=2                # same
CONFIG_LV_USE_CLIB_MALLOC=y       # affects allocator function pointers
CONFIG_LV_DRAW_BUF_ALIGN=4        # affects buffer alignment fields
CONFIG_LV_DRAW_BUF_STRIDE_ALIGN=1
```

These settings are pre-set in the `sdkconfig.defaults` of each example project.
If you start from scratch, copy those defaults into your own project.

Do **not** set `CONFIG_LV_CONF_SKIP=y` in a plugin project. That flag tells
LVGL to use a hand-written `lv_conf.h`, which is what the firmware's board
support package does internally — plugin projects should instead let the
component manager generate `lv_conf.h` from `sdkconfig`.

## Exported symbol constraints

Not every LVGL function is in the firmware's plugin export table. The most
common surprises:

- `lv_color_make(r,g,b)` is NOT exported. Use `lv_color_hex(0xRRGGBB)` instead.
- `lv_arc_set_bg_angles`, `lv_arc_set_range`, `lv_arc_set_value` are NOT
  exported. Use `lv_scale_*` for a full indicator arc with needle support.
- `lv_obj_clear_flag` is a compat macro that expands to `lv_obj_remove_flag`;
  call `lv_obj_remove_flag` directly.
- `lv_timer_del` is a compat macro for `lv_timer_delete`; call
  `lv_timer_delete` directly.
- `lv_img_*` LVGL 8 compat functions are macros, not real symbols; use the
  LVGL 9 `lv_image_*` API.

The authoritative list is `main/plugins/qtune_plugin_symbols.txt` in the
firmware source. The `README.md` in the SDK root reproduces it in full.

## Firmware font exports

The following Montserrat fonts are exported by the firmware and available to
plugins via LVGL's font pointer syntax (`&lv_font_montserrat_NN`):

- `lv_font_montserrat_14` (default body text)
- `lv_font_montserrat_18`
- `lv_font_montserrat_24`
- `lv_font_montserrat_26`
- `lv_font_montserrat_28`
- `lv_font_montserrat_40`
- `lv_font_montserrat_48`

To use a font size not in this list, enable it in your `sdkconfig.defaults`
and add it to your component's sources — the linker will embed the font data
directly into your `.so` instead of resolving it from the firmware.
