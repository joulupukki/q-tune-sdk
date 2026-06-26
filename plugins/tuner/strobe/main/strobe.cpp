/*
 * strobe.cpp — Strobe
 *
 * A Q-Tune TUNER plugin in the style of a classic mechanical / Peterson-type
 * STROBE tuner. Instead of a needle it shows several horizontal bands of
 * vertical stripes that appear to scroll:
 *
 *   - flat  (cents < 0): the stripes drift to the LEFT
 *   - sharp (cents > 0): the stripes drift to the RIGHT
 *   - the further out of tune, the faster they move (it's a blur when way off)
 *   - dead in tune: the pattern FREEZES and snaps to the user's accent colour
 *
 * Each band scrolls at a slightly different rate (like the octave rows on a real
 * strobe), so a locked note produces a striking "everything stops at once" read.
 *
 * Colour discipline: while searching, the stripes are a calm neutral grey; the
 * instant the note is inside the in-tune window they turn the user's chosen accent
 * colour AND stop. So "grey + moving = keep tuning", "colour + frozen = locked".
 *
 * Orientation aware: portrait stacks the note above the strobe panel; landscape
 * puts the note on the left and the panel on the right. All geometry is computed
 * from the live screen_width / screen_height / is_landscape host globals.
 *
 * Everything called here is in docs/ALLOWED_SYMBOLS.md.
 */

#include "qtune_plugin.h"

#include <stdio.h>
#include <math.h>

// --- Interface callbacks (forward declarations) ----------------------------
static const char  *str_get_name(void);
static TuningUIType str_get_type(void);
static void         str_init(lv_obj_t *screen);
static void         str_display_frequency(float frequency,
                                               float target_frequency,
                                               TunerNoteName note_name,
                                               int octave,
                                               float cents,
                                               bool show_mute_indicator);
static void         str_align_settings_button(lv_obj_t *btn);
static void         str_cleanup(void);

static TunerGUIInterface str_interface = {
    .get_name              = str_get_name,
    .get_type              = str_get_type,
    .init                  = str_init,
    .display_frequency     = str_display_frequency,
    .align_settings_button = str_align_settings_button,
    .cleanup               = str_cleanup,
};

// --- Plugin descriptor + entry point (REQUIRED — do not remove) ------------
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_TUNER,
    .sdk_build    = "strobe-1.0",
    .interface    = &str_interface,
    // Stable identity. The firmware assigns the numeric slot dynamically at load;
    // never change this uid once published or the user's saved selection is lost.
    .uid          = "qtune.strobe.gdam5c",
};

QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}

// ---------------------------------------------------------------------------
// Tunable layout / animation constants
// ---------------------------------------------------------------------------
#define NOTE_GLYPH_SIZE   QT_GLYPH_SIZE_MEDIUM   // 100px note artwork

#define NUM_BANDS         4          // horizontal strobe rows
#define MAX_BARS          28         // generous upper bound for the widest screen
#define BAR_W             14         // stripe width  (px)
#define BAR_GAP           14         // gap between stripes (px) -> period = 28
#define BAND_GAP          8          // vertical gap between bands (px)

#define TIMER_MS          25         // ~40 Hz animation tick
#define SPEED_K           0.060f     // px-per-tick per cent (at mult 1.0)
#define SPEED_SMOOTH      0.25f      // ease the scroll speed toward its target

#define CORNER_MARGIN     8

// Per-band speed multipliers — different rates give the layered strobe look.
static const float BAND_MULT[NUM_BANDS] = { 0.5f, 1.0f, 1.5f, 2.25f };

// ---------------------------------------------------------------------------
// State (set in str_init, nulled in str_cleanup)
// ---------------------------------------------------------------------------
static lv_obj_t  *s_screen    = NULL;
static lv_obj_t  *s_panel     = NULL;            // frame around the strobe bands
static lv_obj_t  *s_band[NUM_BANDS]            = { NULL };
static lv_obj_t  *s_bar[NUM_BANDS][MAX_BARS]   = { { NULL } };
static int        s_bar_count[NUM_BANDS]       = { 0 };
static lv_obj_t  *s_note_img  = NULL;
static lv_obj_t  *s_sharp_img = NULL;
static lv_obj_t  *s_mute_img  = NULL;
static lv_obj_t  *s_cents_lbl = NULL;            // optional cents readout
static lv_timer_t *s_timer    = NULL;

static int        s_band_h    = 0;
static int        s_period    = BAR_W + BAR_GAP;
static float      s_phase[NUM_BANDS]  = { 0 };
static float      s_speed             = 0.f;     // smoothed current scroll speed

// Latest pitch snapshot written by display_frequency, read by the timer.
static volatile float        s_cents   = 0.f;
static volatile TunerNoteName s_note   = NOTE_NONE;
static volatile bool         s_in_tune = false;
static volatile bool         s_active  = false;  // a note is present

// Cached visual state to avoid redundant LVGL work.
static int            s_color_state = -1;        // 0 none, 1 searching, 2 locked
static TunerNoteName  s_last_note   = NOTE_NONE;
static bool           s_last_muted  = false;
static bool           s_show_cents  = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static lv_color_t str_accent(void) {
    lv_palette_t p = qt_get_note_name_palette();
    return (p == LV_PALETTE_NONE) ? lv_palette_main(LV_PALETTE_AMBER)
                                  : lv_palette_main(p);
}

static const char *str_get_name(void) { return "Q Strobe"; }
static TuningUIType str_get_type(void) { return TuningUITypeStandard; }

// ---------------------------------------------------------------------------
// Animation tick — advances the scroll phase of every band and repositions the
// stripes. Runs inside the LVGL lock (host holds it); never lock here.
// ---------------------------------------------------------------------------
static void str_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!s_panel) { return; }

    // Target scroll speed: zero when locked or no note, else proportional to the
    // signed cents deviation. Ease toward it so direction changes feel smooth.
    float target = (s_in_tune || !s_active) ? 0.f : (s_cents * SPEED_K);
    s_speed += (target - s_speed) * SPEED_SMOOTH;

    // When locked, gently let any residual motion die out.
    bool frozen = (s_in_tune || !s_active) && (fabsf(s_speed) < 0.05f);
    if (frozen) { s_speed = 0.f; }

    if (s_speed == 0.f) { return; }   // nothing to redraw while fully frozen

    for (int b = 0; b < NUM_BANDS; b++) {
        if (!s_band[b]) { continue; }
        float ph = s_phase[b] + s_speed * BAND_MULT[b];
        // wrap into [0, period)
        float P = (float)s_period;
        while (ph >= P) ph -= P;
        while (ph < 0.f) ph += P;
        s_phase[b] = ph;

        int count = s_bar_count[b];
        for (int i = 0; i < count; i++) {
            if (!s_bar[b][i]) { continue; }
            int x = (int)(i * s_period + ph) - s_period;
            lv_obj_set_x(s_bar[b][i], (lv_coord_t)x);
        }
    }
}

// ---------------------------------------------------------------------------
// Build the strobe panel + bands + stripes inside a given rectangle.
// ---------------------------------------------------------------------------
static void str_build_panel(lv_coord_t px, lv_coord_t py,
                            lv_coord_t pw, lv_coord_t ph) {
    s_panel = lv_obj_create(s_screen);
    lv_obj_set_size(s_panel, pw, ph);
    lv_obj_set_pos(s_panel, px, py);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_panel, 10, 0);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_panel, 2, 0);
    lv_obj_set_style_border_color(s_panel, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_panel, 6, 0);

    int inner_w = pw - 12;                       // minus pad_all*2
    int inner_h = ph - 12;
    s_band_h = (inner_h - (NUM_BANDS - 1) * BAND_GAP) / NUM_BANDS;
    if (s_band_h < 6) s_band_h = 6;

    int count = inner_w / s_period + 2;
    if (count > MAX_BARS) count = MAX_BARS;

    for (int b = 0; b < NUM_BANDS; b++) {
        lv_obj_t *band = lv_obj_create(s_panel);
        s_band[b] = band;
        lv_obj_set_size(band, inner_w, s_band_h);
        lv_obj_set_pos(band, 0, b * (s_band_h + BAND_GAP));
        lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(band, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(band, 3, 0);
        lv_obj_set_style_bg_color(band, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(band, 0, 0);
        lv_obj_set_style_pad_all(band, 0, 0);
        lv_obj_set_style_clip_corner(band, true, 0);

        s_bar_count[b] = count;
        for (int i = 0; i < count; i++) {
            lv_obj_t *bar = lv_obj_create(band);
            s_bar[b][i] = bar;
            lv_obj_set_size(bar, BAR_W, s_band_h);
            lv_obj_set_pos(bar, i * s_period - s_period, 0);
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_radius(bar, 0, 0);
            lv_obj_set_style_border_width(bar, 0, 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0x555555), 0);
        }
    }
}

static void str_init(lv_obj_t *screen) {
    s_screen = screen;
    s_timer  = NULL;
    s_color_state = -1;
    s_last_note   = NOTE_NONE;
    s_last_muted  = false;
    s_in_tune = false;
    s_active  = false;
    s_note    = NOTE_NONE;
    s_cents   = 0.f;
    s_speed   = 0.f;
    s_show_cents = qt_get_show_cents() != 0;
    for (int b = 0; b < NUM_BANDS; b++) { s_phase[b] = (float)(b * 5); }

    // Panel geometry per orientation. The note never overlaps the panel.
    lv_coord_t W = screen_width, H = screen_height;
    if (is_landscape) {
        // Note on the left ~38% width, strobe panel fills the right.
        lv_coord_t note_col = (lv_coord_t)(W * 0.38f);
        str_build_panel(note_col, 12, W - note_col - 12, H - 24);
    } else {
        // Note across the top, strobe panel fills the lower ~62%.
        lv_coord_t panel_top = (lv_coord_t)(H * 0.36f);
        str_build_panel(12, panel_top, W - 24, H - panel_top - 14);
    }

    // Note artwork (letter + sharp overlay).
    s_note_img = lv_image_create(screen);
    lv_image_set_src(s_note_img, qt_get_blank_glyph(NOTE_GLYPH_SIZE));
    lv_obj_set_style_img_recolor_opa(s_note_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_note_img, lv_color_white(), 0);

    s_sharp_img = lv_image_create(screen);
    lv_image_set_src(s_sharp_img, qt_get_sharp_glyph(NOTE_GLYPH_SIZE));
    lv_obj_set_style_img_recolor_opa(s_sharp_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_sharp_img, lv_color_white(), 0);
    lv_obj_add_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);

    if (is_landscape) {
        lv_obj_align(s_note_img, LV_ALIGN_LEFT_MID, 18, 0);
    } else {
        lv_obj_align(s_note_img, LV_ALIGN_TOP_MID, 0, 18);
    }
    lv_obj_align_to(s_sharp_img, s_note_img, LV_ALIGN_OUT_RIGHT_MID, -18, 0);

    // Optional cents readout (only if the user enabled it).
    if (s_show_cents) {
        s_cents_lbl = lv_label_create(screen);
        lv_label_set_text(s_cents_lbl, "");
        lv_obj_set_style_text_font(s_cents_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(s_cents_lbl, lv_color_hex(0x888888), 0);
        lv_obj_align_to(s_cents_lbl, s_note_img, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    }

    // Mute indicator (top-left corner), hidden until reported.
    s_mute_img = lv_image_create(screen);
    lv_image_set_src(s_mute_img, qt_get_mute_glyph());
    lv_obj_set_style_img_recolor_opa(s_mute_img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_mute_img, str_accent(), 0);
    lv_obj_align(s_mute_img, LV_ALIGN_TOP_LEFT, CORNER_MARGIN, CORNER_MARGIN);
    lv_obj_add_flag(s_mute_img, LV_OBJ_FLAG_HIDDEN);

    s_timer = lv_timer_create(str_timer_cb, TIMER_MS, NULL);
}

// Recolour the stripes / panel for the three states.
static void str_apply_color_state(int state) {
    if (state == s_color_state) { return; }
    s_color_state = state;

    lv_color_t bar_col;
    lv_color_t border_col;
    switch (state) {
        case 2:  // locked
            bar_col    = str_accent();
            border_col = str_accent();
            break;
        case 1:  // searching
            bar_col    = lv_color_hex(0x777777);
            border_col = lv_color_hex(0x303030);
            break;
        default: // no note
            bar_col    = lv_color_hex(0x3a3a3a);
            border_col = lv_color_hex(0x303030);
            break;
    }
    for (int b = 0; b < NUM_BANDS; b++) {
        int count = s_bar_count[b];
        for (int i = 0; i < count; i++) {
            if (s_bar[b][i]) { lv_obj_set_style_bg_color(s_bar[b][i], bar_col, 0); }
        }
    }
    if (s_panel) {
        lv_obj_set_style_border_color(s_panel, border_col, 0);
        // A brighter frame when locked makes the freeze unmistakable.
        lv_obj_set_style_border_width(s_panel, state == 2 ? 3 : 2, 0);
    }
}

static void str_display_frequency(float frequency,
                                       float target_frequency,
                                       TunerNoteName note_name,
                                       int octave,
                                       float cents,
                                       bool show_mute_indicator) {
    (void)frequency; (void)target_frequency; (void)octave;
    if (!s_panel) { return; }

    bool active  = (note_name != NOTE_NONE);
    bool in_tune = active && (fabsf(cents) <= (float)qt_get_in_tune_cents_width());

    // Publish to the animation timer.
    s_cents   = cents;
    s_note    = note_name;
    s_active  = active;
    s_in_tune = in_tune;

    // Mute indicator.
    if (show_mute_indicator != s_last_muted) {
        if (show_mute_indicator) lv_obj_remove_flag(s_mute_img, LV_OBJ_FLAG_HIDDEN);
        else                     lv_obj_add_flag(s_mute_img, LV_OBJ_FLAG_HIDDEN);
        s_last_muted = show_mute_indicator;
    }

    // Note artwork on change.
    if (note_name != s_last_note) {
        lv_image_set_src(s_note_img, qt_get_note_glyph(note_name, NOTE_GLYPH_SIZE));
        if (qt_note_is_sharp(note_name)) lv_obj_remove_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);
        else                             lv_obj_add_flag(s_sharp_img, LV_OBJ_FLAG_HIDDEN);
        s_last_note = note_name;
    }

    // Colour state.
    int state = !active ? 0 : (in_tune ? 2 : 1);
    str_apply_color_state(state);

    // Note glyph colour: accent when locked, white when searching, dim when none.
    lv_color_t note_col = in_tune ? str_accent()
                        : active  ? lv_color_white()
                                  : lv_color_hex(0x606060);
    lv_obj_set_style_img_recolor(s_note_img, note_col, 0);
    lv_obj_set_style_img_recolor(s_sharp_img, note_col, 0);

    // Optional cents readout.
    if (s_cents_lbl) {
        if (active) {
            char buf[12];
            snprintf(buf, sizeof(buf), "%+0.1f", (double)cents);
            lv_label_set_text(s_cents_lbl, buf);
        } else {
            lv_label_set_text(s_cents_lbl, "--");
        }
        lv_obj_set_style_text_color(s_cents_lbl,
                                    in_tune ? str_accent() : lv_color_hex(0x888888), 0);
    }
}

static void str_align_settings_button(lv_obj_t *btn) {
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -CORNER_MARGIN, -CORNER_MARGIN);
}

static void str_cleanup(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_screen    = NULL;
    s_panel     = NULL;
    s_note_img  = NULL;
    s_sharp_img = NULL;
    s_mute_img  = NULL;
    s_cents_lbl = NULL;
    for (int b = 0; b < NUM_BANDS; b++) {
        s_band[b] = NULL;
        s_bar_count[b] = 0;
        for (int i = 0; i < MAX_BARS; i++) { s_bar[b][i] = NULL; }
    }
}
