/*
 * @PROJECT_NAME@.cpp — @DISPLAY_NAME@
 *
 * A starter Q-Tune TUNER plugin. It shows the detected note (using the built-in
 * note artwork) and the cents offset, and turns the note green-ish... actually,
 * the user's accent colour, when you're in tune. Use it as a base: keep the
 * descriptor/entry boilerplate and the interface wiring, and change the drawing
 * in @PREFIX@_init() / @PREFIX@_display_frequency() to build your own look.
 *
 * Everything you may call is listed in docs/ALLOWED_SYMBOLS.md. Build with
 * `./docker-build.sh <this folder>`; validate with tools/validate_plugin.py.
 */

#include "qtune_plugin.h"

#include <stdio.h>
#include <math.h>

// --- Interface callbacks (forward declarations) ----------------------------
static const char  *@PREFIX@_get_name(void);
static TuningUIType @PREFIX@_get_type(void);
static void         @PREFIX@_init(lv_obj_t *screen);
static void         @PREFIX@_display_frequency(float frequency,
                                               float target_frequency,
                                               TunerNoteName note_name,
                                               int octave,
                                               float cents,
                                               bool show_mute_indicator);
static void         @PREFIX@_align_settings_button(lv_obj_t *btn);
static void         @PREFIX@_cleanup(void);

static TunerGUIInterface @PREFIX@_interface = {
    .get_name              = @PREFIX@_get_name,
    .get_type              = @PREFIX@_get_type,
    .init                  = @PREFIX@_init,
    .display_frequency     = @PREFIX@_display_frequency,
    .align_settings_button = @PREFIX@_align_settings_button,
    .cleanup               = @PREFIX@_cleanup,
};

// --- Plugin descriptor + entry point (REQUIRED — do not remove) ------------
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_TUNER,
    .sdk_build    = "@SDK_BUILD_TAG@",
    .interface    = &@PREFIX@_interface,
    // Stable identity. The firmware assigns the numeric slot dynamically at load;
    // never change this uid once published or the user's saved selection is lost.
    .uid          = "@PLUGIN_UID@",
};

QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}

// --- Plugin state ----------------------------------------------------------
#define NOTE_GLYPH_SIZE  QT_GLYPH_SIZE_LARGE   // 150px; see qt_glyph_size_t

static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_note_img   = NULL;  // the note letter (A-G / blank)
static lv_obj_t *s_sharp_img  = NULL;  // sharp (#) overlay, hidden for naturals
static lv_obj_t *s_cents_label = NULL;

static TunerNoteName s_last_note = NOTE_NONE;
static bool          s_last_in_tune = false;

// --- Callbacks -------------------------------------------------------------

static const char *@PREFIX@_get_name(void) {
    return "@DISPLAY_NAME@";  // shown in Settings > Tuner UI
}

static TuningUIType @PREFIX@_get_type(void) {
    return TuningUITypeStandard;  // ...Utility = only shows when utility tuners are enabled
}

// Called once when tuning starts. Build all your LVGL objects here.
static void @PREFIX@_init(lv_obj_t *screen) {
    s_screen       = screen;
    s_last_note    = NOTE_NONE;
    s_last_in_tune = false;

    // The note letter, drawn from the firmware's built-in artwork. It's a
    // greyscale mask, so img_recolor tints it.
    s_note_img = lv_image_create(screen);
    lv_image_set_src(s_note_img, qt_get_blank_glyph(NOTE_GLYPH_SIZE));
    lv_obj_set_style_img_recolor_opa(s_note_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_note_img, lv_color_white(), 0);
    lv_obj_align(s_note_img, LV_ALIGN_CENTER, 0, -20);

    // Sharp (#) overlay — hugs the letter's right edge, hidden for naturals.
    s_sharp_img = lv_image_create(screen);
    lv_image_set_src(s_sharp_img, qt_get_sharp_glyph(NOTE_GLYPH_SIZE));
    lv_obj_set_style_img_recolor_opa(s_sharp_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_sharp_img, lv_color_white(), 0);
    lv_obj_align_to(s_sharp_img, s_note_img, LV_ALIGN_OUT_RIGHT_MID, -24, 0);
    lv_obj_add_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);

    // A cents readout below the note.
    s_cents_label = lv_label_create(screen);
    lv_label_set_text(s_cents_label, "--");
    lv_obj_set_style_text_color(s_cents_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_cents_label, &lv_font_montserrat_28, 0);
    lv_obj_align(s_cents_label, LV_ALIGN_CENTER, 0, 90);

    // TODO: make it yours. Ideas: a needle gauge (lv_scale), an arc, a moving
    // bar, an animated background. See plugins/tuner/gauge for a needle gauge,
    // or plugins/tuner/strobe for a strobe display.
}

// Called ~30x/second with the latest pitch. Keep it fast.
static void @PREFIX@_display_frequency(float frequency,
                                       float target_frequency,
                                       TunerNoteName note_name,
                                       int octave,
                                       float cents,
                                       bool show_mute_indicator) {
    (void)frequency; (void)target_frequency; (void)octave; (void)show_mute_indicator;
    if (!s_note_img) { return; }

    // Swap the note artwork only when the note changes (NOTE_NONE => blank).
    if (note_name != s_last_note) {
        lv_image_set_src(s_note_img, qt_get_note_glyph(note_name, NOTE_GLYPH_SIZE));
        if (qt_note_is_sharp(note_name)) {
            lv_obj_remove_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);
        }
        s_last_note = note_name;

        if (note_name == NOTE_NONE) {
            lv_label_set_text(s_cents_label, "--");
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%+d", (int)cents);
            lv_label_set_text(s_cents_label, buf);
        }
    }

    // Tint everything to the user's accent colour when in tune.
    bool in_tune = (note_name != NOTE_NONE) &&
                   (fabsf(cents) <= (float)qt_get_in_tune_cents_width());
    if (in_tune != s_last_in_tune) {
        lv_palette_t palette = qt_get_note_name_palette();
        lv_color_t accent = (palette == LV_PALETTE_NONE)
                                ? lv_palette_main(LV_PALETTE_AMBER)
                                : lv_palette_main(palette);
        lv_color_t colour = in_tune ? accent : lv_color_white();
        lv_obj_set_style_img_recolor(s_note_img, colour, 0);
        lv_obj_set_style_img_recolor(s_sharp_img, colour, 0);
        lv_obj_set_style_text_color(s_cents_label, colour, 0);
        s_last_in_tune = in_tune;
    }
}

// Place the firmware's settings gear button (called once after init).
static void @PREFIX@_align_settings_button(lv_obj_t *btn) {
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
}

// Called when this tuner is deactivated. Delete timers/animations here (this
// starter has none). Do NOT delete children of `screen` — the host does that.
static void @PREFIX@_cleanup(void) {
    s_screen      = NULL;
    s_note_img    = NULL;
    s_sharp_img   = NULL;
    s_cents_label = NULL;
}
