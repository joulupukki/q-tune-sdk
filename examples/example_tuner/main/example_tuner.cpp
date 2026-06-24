// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/**
 * example_tuner.cpp
 *
 * Minimal Q-Tune tuner plugin demonstrating the TunerGUIInterface contract.
 *
 * Layout (portrait, 240x320):
 *   - Large label in the centre showing the note name and octave (e.g. "A4").
 *   - A round lv_scale whose needle sweeps left/right with cents deviation.
 *     The needle turns the user's accent colour when in-tune.
 *   - The firmware's settings gear button is placed via align_settings_button().
 *   - A small reference-frequency badge in the top-right corner.
 *
 * Build output: example_tuner.so  (see CMakeLists.txt / qtune_project_so()).
 * Upload via the /plugins page served over Wi-Fi; the firmware loads plugins
 * from /data/plugins at boot.
 *
 * LANGUAGE: plugins are compiled as C++ (the Q-Tune interface headers pull in
 * tuner_math.h, which is C++). The SDK's qtune_project_so() compiles plugin
 * sources with the C++ compiler using -fno-exceptions -fno-rtti. Keep the
 * descriptor in an extern "C" block so dlsym() finds it by its plain name.
 *
 * SYMBOL DISCIPLINE
 * -----------------
 * Every lv_* call here must be in the firmware export table
 * (main/plugins/qtune_plugin_symbols.txt). A call to a non-exported symbol links
 * fine (undefined symbols are allowed in the .so) but FAILS to resolve at load
 * time, so the plugin will be rejected. Validate with:
 *     xtensa-esp32s3-elf-nm -u build/example_tuner.so
 * and confirm every undefined symbol is in the export table or provided by the
 * loader's libc/ESP-IDF tables.
 */

#include "qtune_plugin.h"

#include <stdio.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static uint8_t      et_get_id(void);
static const char  *et_get_name(void);
static TuningUIType et_get_type(void);
static void         et_init(lv_obj_t *screen);
static void         et_display_frequency(float frequency,
                                          float target_frequency,
                                          TunerNoteName note_name,
                                          int octave,
                                          float cents,
                                          bool show_mute_indicator);
static void         et_align_settings_button(lv_obj_t *btn);
static void         et_cleanup(void);

// ---------------------------------------------------------------------------
// Interface struct
// ---------------------------------------------------------------------------
static TunerGUIInterface et_interface = {
    .get_id                = et_get_id,
    .get_name              = et_get_name,
    .get_type              = et_get_type,
    .init                  = et_init,
    .display_frequency     = et_display_frequency,
    .align_settings_button = et_align_settings_button,
    .cleanup               = et_cleanup,
};

// ---------------------------------------------------------------------------
// Plugin descriptor — exactly one per .so. extern "C" keeps the symbol name
// unmangled; QTUNE_PLUGIN_EXPORT gives it default ELF visibility so it survives
// --strip-all and can be found by the loader's dlsym().
// ---------------------------------------------------------------------------
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_TUNER,
    .sdk_build    = "example-sdk-1.0",
    .interface    = &et_interface,
};

// Entry function the firmware calls at load time. Required: the ELF loader's
// dlsym() finds only function symbols, not the descriptor data object above.
QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}

// ---------------------------------------------------------------------------
// Scale parameters
//
// The lv_scale widget is the most export-complete indicator primitive. We use
// it in ROUND_INNER mode as a circular needle gauge. cents [-50,50] are scaled
// 10x to fill the scale range for good visual resolution.
// ---------------------------------------------------------------------------
#define SCALE_RANGE_MIN    (-500)
#define SCALE_RANGE_MAX    (500)
#define SCALE_SIZE_PCT     80    // % of screen_width

// Module-level state — set in et_init(), nulled in et_cleanup().
static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_note_label = NULL;
static lv_obj_t *s_ref_label  = NULL;
static lv_obj_t *s_scale      = NULL;
static lv_obj_t *s_needle     = NULL;  // lv_line inside the scale

// Cached display state to minimise redundant LVGL calls.
static TunerNoteName s_last_note    = NOTE_NONE;
static int           s_last_octave  = -999;
static float         s_last_cents   = 9999.f;
static bool          s_last_in_tune = false;

// ---------------------------------------------------------------------------
// Interface implementations
// ---------------------------------------------------------------------------

static uint8_t et_get_id(void) {
    // In the reserved tuner plugin range [100, 199]. Pick any unused value.
    return 100;
}

static const char *et_get_name(void) {
    return "Example";
}

static TuningUIType et_get_type(void) {
    return TuningUITypeStandard;
}

static void et_init(lv_obj_t *screen) {
    s_screen = screen;

    // Reset cached state so the first display_frequency call does a full draw.
    s_last_note    = NOTE_NONE;
    s_last_octave  = -999;
    s_last_cents   = 9999.f;
    s_last_in_tune = false;

    // Reference-frequency badge — top-right corner.
    s_ref_label = lv_label_create(screen);
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "A4=%d", (int)qt_get_reference_frequency());
        lv_label_set_text(s_ref_label, buf);
    }
    lv_obj_set_style_text_color(s_ref_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(s_ref_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_ref_label, LV_ALIGN_TOP_RIGHT, -8, 8);

    // Note-name + octave label — centre, shifted up so the scale sits below it.
    s_note_label = lv_label_create(screen);
    lv_label_set_text(s_note_label, "-");
    lv_obj_set_style_text_font(s_note_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_note_label, lv_color_white(), 0);
    lv_obj_align(s_note_label, LV_ALIGN_CENTER, 0, -50);

    // Cents scale — round needle gauge.
    lv_coord_t scale_size = (lv_coord_t)((screen_width * SCALE_SIZE_PCT) / 100);

    s_scale = lv_scale_create(screen);
    lv_obj_set_size(s_scale, scale_size, scale_size);
    lv_scale_set_mode(s_scale, LV_SCALE_MODE_ROUND_INNER);

    lv_obj_set_style_bg_opa(s_scale, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_scale, lv_color_black(), 0);
    lv_obj_set_style_radius(s_scale, LV_RADIUS_CIRCLE, 0);

    lv_scale_set_range(s_scale, SCALE_RANGE_MIN, SCALE_RANGE_MAX);
    lv_scale_set_angle_range(s_scale, 270);  // 270-degree sweep
    lv_scale_set_rotation(s_scale, 135);     // open at the bottom

    lv_scale_set_label_show(s_scale, false);
    lv_scale_set_total_tick_count(s_scale, 11);  // one per 50 cents
    lv_scale_set_major_tick_every(s_scale, 5);   // marks at ±0, ±25, ±50 cents

    lv_obj_set_style_line_color(s_scale, lv_color_hex(0x404040), LV_PART_ITEMS);

    lv_obj_align(s_scale, LV_ALIGN_CENTER, 0, 40);

    // Needle line inside the scale. Local inline styles keep colour updates in
    // display_frequency() to simple one-liners (no shared lv_style_t lifecycle).
    s_needle = lv_line_create(s_scale);
    lv_obj_set_style_line_width(s_needle, 4, LV_PART_MAIN);
    lv_obj_set_style_line_color(s_needle, lv_color_white(), LV_PART_MAIN);

    lv_scale_set_line_needle_value(s_scale, s_needle,
                                   scale_size / 2,   // needle length (to rim)
                                   0);               // initial value
}

static void et_display_frequency(float frequency,
                                  float target_frequency,
                                  TunerNoteName note_name,
                                  int octave,
                                  float cents,
                                  bool show_mute_indicator) {
    (void)frequency;
    (void)target_frequency;
    (void)show_mute_indicator;

    if (!s_note_label || !s_scale || !s_needle) { return; }

    // Update the note + octave label only when the value changes.
    if (note_name != s_last_note || octave != s_last_octave) {
        if (note_name == NOTE_NONE) {
            lv_label_set_text(s_note_label, "-");
        } else {
            char buf[8];
            // name_for_note() is a static inline in tuner_math.h — compiles
            // into the .so, no firmware symbol reference required.
            snprintf(buf, sizeof(buf), "%s%d", name_for_note(note_name), octave);
            lv_label_set_text(s_note_label, buf);
        }
        s_last_note   = note_name;
        s_last_octave = octave;
    }

    // Update needle position when cents change by more than 0.2 ct.
    if (fabsf(cents - s_last_cents) > 0.2f) {
        int32_t scaled = (int32_t)(cents * 10.0f);
        if (scaled < SCALE_RANGE_MIN) scaled = SCALE_RANGE_MIN;
        if (scaled > SCALE_RANGE_MAX) scaled = SCALE_RANGE_MAX;

        lv_coord_t scale_size = (lv_coord_t)((screen_width * SCALE_SIZE_PCT) / 100);
        lv_scale_set_line_needle_value(s_scale, s_needle, scale_size / 2, scaled);
        s_last_cents = cents;
    }

    // Colour the needle and note label when in-tune.
    bool in_tune = (note_name != NOTE_NONE) &&
                   (fabsf(cents) <= (float)qt_get_in_tune_cents_width());

    if (in_tune != s_last_in_tune) {
        lv_palette_t palette = qt_get_note_name_palette();
        lv_color_t accent = (palette == LV_PALETTE_NONE)
                                ? lv_palette_main(LV_PALETTE_AMBER)
                                : lv_palette_main(palette);
        lv_color_t indicator_color = in_tune ? accent : lv_color_white();

        lv_obj_set_style_line_color(s_needle, indicator_color, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_note_label, indicator_color, 0);
        s_last_in_tune = in_tune;
    }
}

static void et_align_settings_button(lv_obj_t *btn) {
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
}

static void et_cleanup(void) {
    // The host deletes all children of `screen` after cleanup(); we only null
    // our pointers. This example creates no timers/animations to tear down.
    s_screen     = NULL;
    s_note_label = NULL;
    s_ref_label  = NULL;
    s_scale      = NULL;
    s_needle     = NULL;
}
