// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/**
 * example_tuner.cpp
 *
 * Minimal Q-Tune tuner plugin demonstrating the TunerGUIInterface contract.
 *
 * Layout — orientation-aware (the tuner can run portrait OR landscape). The note
 * and the gauge are placed on opposite ends so they never overlap:
 *   - Portrait (240x320):  note across the TOP,  gauge anchored to the BOTTOM.
 *   - Landscape (320x240): note down the LEFT,   gauge anchored to the RIGHT.
 * is_landscape (a host global) selects the arrangement at init; et_scale_size()
 * keeps the gauge sized to the shorter edge so it always fits.
 *
 * Elements:
 *   - The detected note, rendered with the firmware's built-in note artwork:
 *     a letter glyph (A-G) plus a sharp (#) overlay for accidentals, fetched via
 *     qt_get_note_glyph() / qt_get_sharp_glyph(). Both are recoloured to the
 *     user's accent colour when in-tune.
 *   - A round lv_scale needle gauge whose needle sweeps left/right with cents
 *     deviation and turns the accent colour when in-tune.
 *   - The firmware's settings gear button is placed via align_settings_button().
 *   - A small reference-frequency badge in the top-right corner.
 *
 * The note glyphs live in firmware flash; the plugin references them through the
 * host accessors rather than embedding its own image assets.
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
    // Stable identity. The firmware assigns the numeric slot dynamically at load;
    // the uid is what persists the user's selection — never change it once shipped.
    .uid          = "qtune.example-tuner.0001",
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

// The gauge diameter is a percentage of the SHORTER screen edge. Landscape uses
// a slightly smaller gauge so the note column fits comfortably beside it.
#define SCALE_SIZE_PCT_PORTRAIT   72
#define SCALE_SIZE_PCT_LANDSCAPE  66

// Layout margins. Portrait stacks vertically (note on top, gauge on the bottom);
// landscape places them side by side (note on the left, gauge on the right). In
// both cases the note and gauge never overlap.
#define NOTE_TOP_MARGIN     20   // portrait: px from the top edge to the note
#define SCALE_BOTTOM_MARGIN 16   // portrait: px from the bottom edge to the gauge
#define NOTE_SIDE_MARGIN    12   // landscape: px in from the left edge to the note
#define SCALE_SIDE_MARGIN    8   // landscape: px in from the right edge to the gauge

// Note artwork size. MEDIUM is the 100x100 set the built-in Meter uses; the
// sharp overlay is the same size and aligns to the letter's right edge.
#define NOTE_GLYPH_SIZE    QT_GLYPH_SIZE_MEDIUM

// Module-level state — set in et_init(), nulled in et_cleanup().
static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_note_img   = NULL;  // letter glyph (A-G / blank)
static lv_obj_t *s_sharp_img  = NULL;  // sharp (#) overlay, hidden for naturals
static lv_obj_t *s_ref_label  = NULL;
static lv_obj_t *s_scale      = NULL;
static lv_obj_t *s_needle     = NULL;  // lv_line inside the scale

// Cached display state to minimise redundant LVGL calls.
static TunerNoteName s_last_note    = NOTE_NONE;
static float         s_last_cents   = 9999.f;
static bool          s_last_in_tune = false;

// ---------------------------------------------------------------------------
// Interface implementations
// ---------------------------------------------------------------------------

// Gauge diameter, based on the SHORTER screen edge so the circle always fits
// regardless of orientation. Used in both et_init() and et_display_frequency()
// so the needle length stays in sync with the widget size.
static lv_coord_t et_scale_size(void) {
    lv_coord_t base = (screen_width < screen_height) ? screen_width : screen_height;
    int pct = is_landscape ? SCALE_SIZE_PCT_LANDSCAPE : SCALE_SIZE_PCT_PORTRAIT;
    return (lv_coord_t)((base * pct) / 100);
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

    // Note artwork — placed so the gauge never overlaps it: across the top in
    // portrait, down the left side in landscape. We start on the blank glyph;
    // display_frequency() swaps in the live letter. The images are greyscale
    // masks, so img_recolor tints them (white now, accent in-tune).
    s_note_img = lv_image_create(screen);
    lv_image_set_src(s_note_img, qt_get_blank_glyph(NOTE_GLYPH_SIZE));
    lv_obj_set_style_img_recolor_opa(s_note_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_note_img, lv_color_white(), 0);
    if (is_landscape) {
        lv_obj_align(s_note_img, LV_ALIGN_LEFT_MID, NOTE_SIDE_MARGIN, 0);
    } else {
        lv_obj_align(s_note_img, LV_ALIGN_TOP_MID, 0, NOTE_TOP_MARGIN);
    }

    // Sharp (#) overlay — hugs the right edge of the letter, hidden for naturals.
    s_sharp_img = lv_image_create(screen);
    lv_image_set_src(s_sharp_img, qt_get_sharp_glyph(NOTE_GLYPH_SIZE));
    lv_obj_set_style_img_recolor_opa(s_sharp_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_sharp_img, lv_color_white(), 0);
    lv_obj_align_to(s_sharp_img, s_note_img, LV_ALIGN_OUT_RIGHT_MID, -16, 0);
    lv_obj_add_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);

    // Cents scale — round needle gauge, opposite the note: bottom edge in
    // portrait, right edge in landscape.
    lv_coord_t scale_size = et_scale_size();

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

    if (is_landscape) {
        lv_obj_align(s_scale, LV_ALIGN_RIGHT_MID, -SCALE_SIDE_MARGIN, 0);
    } else {
        lv_obj_align(s_scale, LV_ALIGN_BOTTOM_MID, 0, -SCALE_BOTTOM_MARGIN);
    }

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
    (void)octave;
    (void)show_mute_indicator;

    if (!s_note_img || !s_sharp_img || !s_scale || !s_needle) { return; }

    // Swap in the note artwork only when the note changes. qt_get_note_glyph()
    // returns the bare letter (the blank glyph for NOTE_NONE); the sharp overlay
    // is shown separately for accidentals, exactly as the built-in tuners do.
    if (note_name != s_last_note) {
        lv_image_set_src(s_note_img, qt_get_note_glyph(note_name, NOTE_GLYPH_SIZE));
        if (qt_note_is_sharp(note_name)) {
            lv_obj_remove_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);
        }
        s_last_note = note_name;
    }

    // Update needle position when cents change by more than 0.2 ct.
    if (fabsf(cents - s_last_cents) > 0.2f) {
        int32_t scaled = (int32_t)(cents * 10.0f);
        if (scaled < SCALE_RANGE_MIN) scaled = SCALE_RANGE_MIN;
        if (scaled > SCALE_RANGE_MAX) scaled = SCALE_RANGE_MAX;

        lv_scale_set_line_needle_value(s_scale, s_needle, et_scale_size() / 2, scaled);
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
        lv_obj_set_style_img_recolor(s_note_img, indicator_color, 0);
        lv_obj_set_style_img_recolor(s_sharp_img, indicator_color, 0);
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
    s_note_img   = NULL;
    s_sharp_img  = NULL;
    s_ref_label  = NULL;
    s_scale      = NULL;
    s_needle     = NULL;
}
