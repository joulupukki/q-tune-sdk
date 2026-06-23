// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// qtune_plugin_host_api.h
//
// The curated Q-Tune host API surface a plugin may call, BEYOND the LVGL functions
// and the standard libc/math/ESP-IDF symbols that elf_loader already exports. Every
// symbol declared here is added to the loader's export table (qtune_plugin_exports.c)
// and shipped to plugin authors in the SDK. Keep it small, C-linkage, and stable.
//
// Globals and accessors here intentionally avoid exposing the C++ UserSettings ABI:
// plugins read live tuner state through plain C accessors, never the settings object.

#ifndef QTUNE_PLUGIN_HOST_API_H
#define QTUNE_PLUGIN_HOST_API_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Live screen geometry (POD globals owned by tuner_gui_task) -------------
extern lv_coord_t screen_width;   // current screen width in px
extern lv_coord_t screen_height;  // current screen height in px
extern bool       is_landscape;   // true when width > height

// --- User-setting accessors (read-only snapshots of the relevant settings) ---
// Reference pitch (A4) in Hz, e.g. 440.
int32_t      qt_get_reference_frequency(void);
// Half-width in cents of the "in tune" window, e.g. 3.
uint8_t      qt_get_in_tune_cents_width(void);
// Non-zero when monitoring mode (buffered bypass + monitoring) is enabled.
uint8_t      qt_get_monitoring_mode(void);
// User's chosen note-name color palette (LV_PALETTE_NONE => retro/amber scheme).
lv_palette_t qt_get_note_name_palette(void);
// Non-zero when the user wants the cents value displayed.
uint8_t      qt_get_show_cents(void);

#ifdef __cplusplus
}
#endif

#endif // QTUNE_PLUGIN_HOST_API_H
