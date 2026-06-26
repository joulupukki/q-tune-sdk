/*
 * hyperdrive.cpp — Hyperdrive
 *
 * A Q-Tune STANDBY plugin: a warp-speed starfield. Stars streak outward from a
 * central vanishing point, accelerating and lengthening into light-trails as they
 * rush past — the classic "jump to hyperspace" screensaver, irresistible on a
 * little glowing pedal on a shop shelf.
 *
 *   - Tap anywhere and the field surges into a warp boost that eases back down.
 *   - Play a note and the stars take on the user's accent colour (and push a
 *     little faster) — so strumming literally drives the ship.
 *   - Each note struck also launches its letter (the "asteroid") out of the
 *     vanishing point toward you, fading as it flies — handy as a glanceable
 *     pitch cue when the pedal is monitoring in buffered-bypass mode.
 *
 * Each star is an lv_line whose two endpoints are recomputed every frame, so the
 * trails get longer toward the edges for a real sense of speed.
 *
 * Orientation aware (240x320 / 320x240): the vanishing point and travel limits
 * come from the live screen geometry. One lv_timer, deleted in cleanup().
 *
 * Everything called here is in docs/ALLOWED_SYMBOLS.md.
 */

#include "qtune_plugin.h"

#include <stdio.h>
#include <math.h>

// --- Interface callbacks (forward declarations) ----------------------------
static const char  *hyp_get_name(void);
static bool         hyp_enable_screen(void);
static void         hyp_init(lv_obj_t *screen);
static void         hyp_cleanup(void);
static void         hyp_display_frequency(float frequency,
                                               float target_frequency,
                                               TunerNoteName note_name,
                                               int octave,
                                               float cents);

static TunerStandbyGUIInterface hyp_interface = {
    .get_name          = hyp_get_name,
    .enable_screen     = hyp_enable_screen,
    .init              = hyp_init,
    .cleanup           = hyp_cleanup,
    .display_frequency = hyp_display_frequency,
};

// --- Plugin descriptor + entry point (REQUIRED — do not remove) ------------
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_STANDBY,
    .sdk_build    = "hyperdrive-1.0",
    .interface    = &hyp_interface,
    // Stable identity. The firmware assigns the numeric slot dynamically at load;
    // never change this uid once published or the user's saved selection is lost.
    .uid          = "qtune.hyperdrive.vxggzt",
};

QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}

// ---------------------------------------------------------------------------
#define NSTARS    44
#define TIMER_MS  40              // ~25 fps
#define ACCEL     1.045f          // radial growth per frame (perspective)

#define ASTER_GLYPH  QT_GLYPH_SIZE_SMALL    // small note-letter artwork for the asteroid
#define ASTER_PX     50                     // pixel size of ASTER_GLYPH
#define ASTER_SHARP_DX (ASTER_PX - 9)       // sharp overlay offset (scaled from the tuners' 100px)

typedef struct {
    float a;     // angle (radians) — fixed travel direction
    float ca, sa;
    float r;     // distance from centre
    float seed;  // per-star speed jitter
} star_t;

static lv_obj_t   *s_bg   = NULL;
static lv_obj_t   *s_star[NSTARS] = { NULL };
static lv_obj_t   *s_hint = NULL;
static lv_timer_t *s_timer = NULL;

static lv_point_precise_t s_pts[NSTARS][2];
static star_t s_st[NSTARS];
static int    s_color_key[NSTARS];   // cached colour bucket (-1 = unset)

static int   s_cx = 0, s_cy = 0;
static float s_maxR = 1.0f;
static float s_boost = 1.0f;          // eased speed multiplier (>1 = warp)
static bool  s_playing = false;
static bool  s_last_playing = false;

// Note-name "asteroid": the struck note's letter flies out toward the viewer.
static lv_obj_t     *s_aster       = NULL;   // note letter image
static lv_obj_t     *s_aster_sharp = NULL;   // sharp (#) overlay
static float         s_aster_r     = 0.f;    // distance from centre
static float         s_aster_ca = 0.f, s_aster_sa = 0.f;  // travel direction
static float         s_aster_life  = 0.f;    // 1 -> 0 over its flight; <= 0 = idle
static TunerNoteName s_aster_note  = NOTE_NONE;

// ---------------------------------------------------------------------------
static lv_color_t hyp_accent(void) {
    lv_palette_t p = qt_get_note_name_palette();
    return (p == LV_PALETTE_NONE) ? lv_palette_main(LV_PALETTE_AMBER)
                                  : lv_palette_main(p);
}

static const char *hyp_get_name(void) { return "Hyperdrive"; }
static bool hyp_enable_screen(void) { return true; }

static void hyp_spawn(int i, bool near_centre) {
    star_t *s = &s_st[i];
    float a = (float)(qt_random_u32() % 62832) / 10000.0f;   // 0..2pi
    s->a  = a;
    s->ca = cosf(a);
    s->sa = sinf(a);
    s->r  = near_centre ? (2.0f + (float)(qt_random_u32() % 30))
                        : (float)(qt_random_u32() % (int)s_maxR);
    s->seed = 0.6f + (float)(qt_random_u32() % 100) / 100.0f;
    s_color_key[i] = -1;
}

// ---------------------------------------------------------------------------
static void hyp_tick(lv_timer_t *timer) {
    (void)timer;
    if (!s_star[0]) { return; }

    // Re-read geometry each frame so the field re-centres if the screen rotates.
    int W = (int)screen_width, H = (int)screen_height;
    s_cx = W / 2;
    s_cy = H / 2;
    s_maxR = sqrtf((float)(s_cx * s_cx + s_cy * s_cy)) + 4.0f;

    // Warp boost eases back toward 1.0 (a tap bumps it up). Playing adds a push.
    float rest = s_playing ? 1.35f : 1.0f;
    s_boost += (rest - s_boost) * 0.06f;

    lv_color_t base = s_playing ? hyp_accent() : lv_color_white();

    for (int i = 0; i < NSTARS; i++) {
        star_t *s = &s_st[i];
        // Accelerate outward (perspective: faster as it gets closer to us).
        s->r = s->r * ACCEL + (1.4f * s->seed * s_boost);
        if (s->r >= s_maxR) { hyp_spawn(i, true); continue; }

        float len = s->r * 0.16f * s_boost + 2.0f;
        float inner = s->r - len;
        if (inner < 0) inner = 0;

        s_pts[i][0].x = (lv_value_precise_t)(s_cx + s->ca * inner);
        s_pts[i][0].y = (lv_value_precise_t)(s_cy + s->sa * inner);
        s_pts[i][1].x = (lv_value_precise_t)(s_cx + s->ca * s->r);
        s_pts[i][1].y = (lv_value_precise_t)(s_cy + s->sa * s->r);
        lv_line_set_points(s_star[i], s_pts[i], 2);

        // Brightness by distance: dim near centre, bright at the rim.
        float frac = s->r / s_maxR;             // 0..1
        int bucket = (int)(frac * 4.0f);        // 0..4
        int key = bucket * 2 + (s_playing ? 1 : 0);
        if (key != s_color_key[i]) {
            s_color_key[i] = key;
            lv_opa_t dark = (lv_opa_t)(190 - bucket * 45);   // brighter at rim
            lv_obj_set_style_line_color(s_star[i], lv_color_darken(base, dark), LV_PART_MAIN);
        }
    }

    // Note-name asteroid: fly the struck note out toward the viewer, fading.
    if (s_aster && s_aster_life > 0.f) {
        s_aster_r = s_aster_r * 1.07f + 2.5f;     // accelerate outward
        s_aster_life -= 0.02f;                    // ~2 s flight at 25 fps
        int nx = s_cx - ASTER_PX / 2 + (int)(s_aster_ca * s_aster_r);
        int ny = s_cy - ASTER_PX / 2 + (int)(s_aster_sa * s_aster_r);
        lv_obj_set_pos(s_aster, nx, ny);
        lv_obj_set_pos(s_aster_sharp, nx + ASTER_SHARP_DX, ny);
        lv_opa_t opa = s_aster_life > 0.f ? (lv_opa_t)(s_aster_life * 255.f) : 0;
        lv_obj_set_style_opa(s_aster, opa, 0);
        lv_obj_set_style_opa(s_aster_sharp, opa, 0);
        if (s_aster_life <= 0.f) {
            lv_obj_add_flag(s_aster, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_aster_sharp, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void hyp_on_touch(lv_event_t *e) {
    (void)e;
    s_boost = 3.2f;   // warp surge; hyp_tick eases it back down
}

static void hyp_init(lv_obj_t *screen) {
    s_timer = NULL;
    s_boost = 1.0f;
    s_playing = false;
    s_last_playing = false;

    int W = (int)screen_width, H = (int)screen_height;
    s_cx = W / 2;
    s_cy = H / 2;
    s_maxR = sqrtf((float)(s_cx * s_cx + s_cy * s_cy)) + 4.0f;

    int big = (W > H ? W : H) + 40;
    s_bg = lv_obj_create(screen);
    lv_obj_set_size(s_bg, big, big);
    lv_obj_set_pos(s_bg, 0, 0);
    lv_obj_remove_flag(s_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_bg, 0, 0);
    lv_obj_set_style_border_width(s_bg, 0, 0);
    lv_obj_set_style_bg_color(s_bg, lv_color_hex(0x05060a), 0);
    lv_obj_set_style_bg_grad_color(s_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_dir(s_bg, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_bg, LV_OPA_COVER, 0);

    for (int i = 0; i < NSTARS; i++) {
        hyp_spawn(i, false);
        // Initialise both endpoints so the first frame is valid.
        s_pts[i][0].x = s_pts[i][1].x = (lv_value_precise_t)s_cx;
        s_pts[i][0].y = s_pts[i][1].y = (lv_value_precise_t)s_cy;

        lv_obj_t *ln = lv_line_create(screen);
        s_star[i] = ln;
        lv_obj_set_style_line_width(ln, 2, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(ln, true, LV_PART_MAIN);
        lv_obj_set_style_line_color(ln, lv_color_white(), LV_PART_MAIN);
        lv_line_set_points(ln, s_pts[i], 2);
    }

    s_hint = lv_label_create(screen);
    lv_label_set_text(s_hint, "tap for warp");
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x3c424c), 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Note-name asteroid (topmost, hidden until a note is struck).
    s_aster = lv_image_create(screen);
    lv_image_set_src(s_aster, qt_get_blank_glyph(ASTER_GLYPH));
    lv_obj_set_style_img_recolor_opa(s_aster, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_aster, LV_OBJ_FLAG_HIDDEN);

    s_aster_sharp = lv_image_create(screen);
    lv_image_set_src(s_aster_sharp, qt_get_sharp_glyph(ASTER_GLYPH));
    lv_obj_set_style_img_recolor_opa(s_aster_sharp, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_aster_sharp, LV_OBJ_FLAG_HIDDEN);

    s_aster_note = NOTE_NONE;
    s_aster_life = 0.f;

    lv_obj_add_event_cb(screen, hyp_on_touch, LV_EVENT_PRESSED, NULL);
    s_timer = lv_timer_create(hyp_tick, TIMER_MS, NULL);
}

static void hyp_cleanup(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_bg = NULL; s_hint = NULL;
    s_aster = NULL; s_aster_sharp = NULL;
    s_aster_note = NOTE_NONE; s_aster_life = 0.f;
    for (int i = 0; i < NSTARS; i++) { s_star[i] = NULL; }
}

static void hyp_display_frequency(float frequency,
                                       float target_frequency,
                                       TunerNoteName note_name,
                                       int octave,
                                       float cents) {
    (void)frequency; (void)target_frequency; (void)octave; (void)cents;
    s_playing = (note_name != NOTE_NONE);
    if (s_playing != s_last_playing) {
        // Force a colour refresh on the next tick by clearing the cache.
        for (int i = 0; i < NSTARS; i++) { s_color_key[i] = -1; }
        s_last_playing = s_playing;
    }

    // Launch the asteroid on a new note (re-arm once the note has been released,
    // so replaying the same pitch fires again).
    if (note_name == NOTE_NONE) {
        s_aster_note = NOTE_NONE;
    } else if (note_name != s_aster_note && s_aster) {
        s_aster_note = note_name;
        lv_image_set_src(s_aster, qt_get_note_glyph(note_name, ASTER_GLYPH));
        lv_color_t c = hyp_accent();
        lv_obj_set_style_img_recolor(s_aster, c, 0);
        lv_obj_remove_flag(s_aster, LV_OBJ_FLAG_HIDDEN);
        if (qt_note_is_sharp(note_name)) {
            lv_obj_set_style_img_recolor(s_aster_sharp, c, 0);
            lv_obj_remove_flag(s_aster_sharp, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_aster_sharp, LV_OBJ_FLAG_HIDDEN);
        }
        float ang = (float)(qt_random_u32() % 62832) / 10000.0f;   // 0..2pi
        s_aster_ca = cosf(ang);
        s_aster_sa = sinf(ang);
        s_aster_r  = 8.0f;
        s_aster_life = 1.0f;
        // Place at the start point now so it doesn't flash at the previous spot.
        int nx = s_cx - ASTER_PX / 2 + (int)(s_aster_ca * s_aster_r);
        int ny = s_cy - ASTER_PX / 2 + (int)(s_aster_sa * s_aster_r);
        lv_obj_set_pos(s_aster, nx, ny);
        lv_obj_set_pos(s_aster_sharp, nx + ASTER_SHARP_DX, ny);
        lv_obj_set_style_opa(s_aster, LV_OPA_COVER, 0);
        lv_obj_set_style_opa(s_aster_sharp, LV_OPA_COVER, 0);
    }
}
