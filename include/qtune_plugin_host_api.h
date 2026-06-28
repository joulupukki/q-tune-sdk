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
#include "tuner_math.h"   // TunerNoteName

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
// Current output bypass type: tunerBypassTypeTrue (mechanical/relay bypass — the
// tuner & standby screens receive no signal, so no pitch is detected) or
// tunerBypassTypeBuffered (signal is buffered through, so pitch stays available
// even when not actively tuning). See TunerBypassType in tuner_math.h.
TunerBypassType qt_get_bypass_type(void);
// User's chosen note-name color palette (LV_PALETTE_NONE => retro/amber scheme).
lv_palette_t qt_get_note_name_palette(void);
// Non-zero when the user wants the cents value displayed.
uint8_t      qt_get_show_cents(void);

// --- Built-in note-name glyph images ----------------------------------------
// The built-in tuner displays render note names as pre-rasterised images (one
// per letter A-G), optionally overlaid with a separate sharp (#) glyph. These
// accessors hand a plugin the very same images, so it can reproduce the stock
// look without embedding its own copies. The images already live in firmware
// flash, so referencing them costs the plugin no extra storage.
//
// Compose a note exactly as the built-ins do:
//     const lv_image_dsc_t *letter = qt_get_note_glyph(note, size);
//     lv_image_set_src(note_img, letter);
//     if (qt_note_is_sharp(note)) {
//         lv_image_set_src(sharp_img, qt_get_sharp_glyph(size));
//     }

// Pixel sizes of the available note-glyph image sets.
typedef enum {
    QT_GLYPH_SIZE_SMALL  = 0,  // 50x50 px
    QT_GLYPH_SIZE_MEDIUM = 1,  // 100x100 px
    QT_GLYPH_SIZE_LARGE  = 2,  // 150x150 px
    QT_GLYPH_SIZE_XLARGE = 3,  // 200x200 px
} qt_glyph_size_t;

// Letter glyph (A-G) for a note at the given size. Accidentals are ignored:
// NOTE_A_SHARP returns the 'A' image — overlay qt_get_sharp_glyph() yourself.
// Returns the blank glyph for NOTE_NONE, or NULL for an unknown size.
const lv_image_dsc_t *qt_get_note_glyph(TunerNoteName note, qt_glyph_size_t size);

// The sharp (#) overlay glyph at the given size, or NULL for an unknown size.
const lv_image_dsc_t *qt_get_sharp_glyph(qt_glyph_size_t size);

// The blank / "no note" glyph at the given size, or NULL for an unknown size.
const lv_image_dsc_t *qt_get_blank_glyph(qt_glyph_size_t size);

// Non-zero when `note` is a sharp (A#, C#, D#, F#, G#).
uint8_t qt_note_is_sharp(TunerNoteName note);

// The built-in "mute" indicator icon (shown when the tuner interface passes
// show_mute_indicator). A single fixed-size image, like the one the stock
// tuner UIs draw. Returns a stable pointer; never NULL.
const lv_image_dsc_t *qt_get_mute_glyph(void);

// --- Misc utilities ---------------------------------------------------------
// Milliseconds since the device booted. Monotonic, never goes backwards, and
// wraps after ~49.7 days. Use it for animation timing and elapsed-time logic
// (e.g. "how long has this note been held?"). Prefer this over lv_timer for
// frame-delta math; pair it with an lv_timer that fires your update callback.
uint32_t qt_uptime_ms(void);

// A 32-bit hardware random number. Good for particle effects, quiz prompts,
// jitter, and any "pick something at random" behaviour. Not for cryptography.
uint32_t qt_random_u32(void);

#ifdef __cplusplus
}
#endif

#endif // QTUNE_PLUGIN_HOST_API_H
