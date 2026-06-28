// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// qtune_plugin.h
//
// Convenience umbrella header for Q-Tune UI plugins.
//
// A plugin only needs:
//   #include "qtune_plugin.h"
//
// This pulls in, in order:
//   1. lvgl.h          — full LVGL 9.2.x API (widgets, animations, timers, etc.)
//   2. defines.h       — public SDK constants (reference freq, cents defaults, etc.)
//   3. tuner_math.h    — TunerNoteName enum, name_for_note(), FrequencyInfo
//   4. tuner_ui_interface.h        — TunerGUIInterface (for tuner plugins)
//   5. tuner_standby_ui_interface.h — TunerStandbyGUIInterface (for standby plugins)
//   6. qtune_plugin_host_api.h     — screen_width/height, qt_get_*() accessors
//   7. qtune_plugin_state_api.h    — qt_state_*() persistent storage (save state)
//   8. qtune_plugin_abi.h          — QTunePluginDescriptor, QTUNE_PLUGIN_EXPORT, ID ranges
//
// IMPORTANT: NOT ALL LVGL FUNCTIONS ARE EXPORTED
// ---------------------------------------------------
// The firmware exports a curated subset of LVGL symbols. Calling an lv_*
// function that is not in the firmware's export table will prevent the plugin
// from loading. Key constraints:
//   - lv_color_make(r,g,b) is NOT exported. Use lv_color_hex(0xRRGGBB).
//   - lv_arc_set_bg_angles / lv_arc_set_range / lv_arc_set_value are NOT
//     exported. Use lv_scale_* widgets for full needle/gauge functionality.
//   - lv_obj_clear_flag is a compat macro; call lv_obj_remove_flag() directly.
//   - lv_timer_del is a compat macro; call lv_timer_delete() directly.
//   - lv_img_* LVGL 8 compat macros have no ELF symbols; use lv_image_*.
// See docs/ALLOWED_SYMBOLS.md (and include/README.md) for the full export list.

// NOTE ON lv_conf.h
// -----------------
// LVGL 9 requires a matching lv_conf.h to be on the include path. Your plugin
// project pulls lvgl/lvgl via the IDF component manager (pinned to "==9.2.2"),
// and the component's Kconfig drives all LV_* settings. The sdkconfig.defaults
// shipped with each example project mirrors the firmware's required settings:
//
//   CONFIG_LV_COLOR_DEPTH=16          (16-bit RGB565 — mandatory)
//   CONFIG_LV_USE_OS=2                (FreeRTOS integration)
//   CONFIG_LV_FONT_MONTSERRAT_14=y    (minimum built-in font)
//   CONFIG_LV_USE_ARC=y
//   CONFIG_LV_USE_LABEL=y
//   CONFIG_LV_USE_LINE=y
//   CONFIG_LV_USE_SCALE=y
//
// If you create a project from scratch (not from the examples), ensure the
// settings above are set in your sdkconfig before building the .so, otherwise
// the binary will be built against a different LVGL layout than the firmware
// and will be rejected at load time.

#pragma once

// LVGL must come first; everything else depends on lv_types.
#include "lvgl.h"

// Public SDK constants (does NOT include GPIO / task / ADC internals).
#include "defines.h"

// tuner_math.h is already pulled in by defines.h. It is written to the common
// C/C++ subset, so both C and C++ plugins get TunerNoteName / name_for_note().
#include "tuner_math.h"

// Plugin interface structs — include both; a plugin implements exactly one.
#include "tuner_ui_interface.h"
#include "tuner_standby_ui_interface.h"

// Host API: screen geometry globals and qt_get_*() accessors.
#include "qtune_plugin_host_api.h"

// Persistent storage: qt_state_*() save/load (NVS-backed; commit sparingly).
#include "qtune_plugin_state_api.h"

// ABI descriptor type, QTUNE_PLUGIN_EXPORT, and reserved ID macros.
#include "qtune_plugin_abi.h"
