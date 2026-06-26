/*
 * @PROJECT_NAME@.cpp — @DISPLAY_NAME@
 *
 * A starter Q-Tune STANDBY plugin (the idle/screensaver screen). It drifts a
 * label around the screen with an lv_timer, tints it when a note is detected,
 * and jumps it to wherever you touch the screen. Use it as a base: keep the
 * descriptor/entry boilerplate and the interface wiring, and change the drawing
 * and the timer callback to build your own animation.
 *
 * IMPORTANT: a standby plugin almost always uses an lv_timer. You MUST delete it
 * in cleanup() (see @PREFIX@_cleanup) or the firmware will crash when it leaves
 * the screen. Everything you may call is in docs/ALLOWED_SYMBOLS.md.
 */

#include "qtune_plugin.h"

#include <stdio.h>

// --- Interface callbacks (forward declarations) ----------------------------
static const char  *@PREFIX@_get_name(void);
static bool         @PREFIX@_enable_screen(void);
static void         @PREFIX@_init(lv_obj_t *screen);
static void         @PREFIX@_cleanup(void);
static void         @PREFIX@_display_frequency(float frequency,
                                               float target_frequency,
                                               TunerNoteName note_name,
                                               int octave,
                                               float cents);

static TunerStandbyGUIInterface @PREFIX@_interface = {
    .get_name          = @PREFIX@_get_name,
    .enable_screen     = @PREFIX@_enable_screen,
    .init              = @PREFIX@_init,
    .cleanup           = @PREFIX@_cleanup,
    .display_frequency = @PREFIX@_display_frequency,
};

// --- Plugin descriptor + entry point (REQUIRED — do not remove) ------------
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_STANDBY,
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
#define TIMER_MS  33   // ~30 fps

static lv_obj_t   *s_label = NULL;
static lv_timer_t *s_timer = NULL;
static int s_x = 20, s_y = 20;     // label position
static int s_dx = 2,  s_dy = 2;    // velocity
static bool s_tinted = false;

// Timer callback: drift the label and bounce off the walls. Runs inside the
// LVGL lock the host already holds — do NOT call lvgl_port_lock() here.
static void @PREFIX@_tick(lv_timer_t *timer) {
    (void)timer;
    if (!s_label) { return; }

    s_x += s_dx;
    s_y += s_dy;

    int right  = (int)screen_width  - lv_obj_get_width(s_label);
    int bottom = (int)screen_height - lv_obj_get_height(s_label);
    if (s_x <= 0)      { s_x = 0;      s_dx = -s_dx; }
    if (s_x >= right)  { s_x = right;  s_dx = -s_dx; }
    if (s_y <= 0)      { s_y = 0;      s_dy = -s_dy; }
    if (s_y >= bottom) { s_y = bottom; s_dy = -s_dy; }

    lv_obj_set_pos(s_label, (lv_coord_t)s_x, (lv_coord_t)s_y);
}

// Touch callback: jump the label to where the user pressed.
static void @PREFIX@_on_touch(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev || !s_label) { return; }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    s_x = (int)p.x;
    s_y = (int)p.y;
    lv_obj_set_pos(s_label, (lv_coord_t)s_x, (lv_coord_t)s_y);
}

// --- Callbacks -------------------------------------------------------------

static const char *@PREFIX@_get_name(void) {
    return "@DISPLAY_NAME@";  // shown in Settings > Standby Screen
}

static bool @PREFIX@_enable_screen(void) {
    return true;  // true = keep the display on; false = blank screen (lowest power)
}

// Called when standby begins. Build objects + start your timer here.
static void @PREFIX@_init(lv_obj_t *screen) {
    s_x = 20; s_y = 20; s_dx = 2; s_dy = 2; s_tinted = false;

    s_label = lv_label_create(screen);
    lv_label_set_text(s_label, "@DISPLAY_NAME@");
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(s_label, (lv_coord_t)s_x, (lv_coord_t)s_y);

    // React to screen touches (the pedal has a touchscreen).
    lv_obj_add_event_cb(screen, @PREFIX@_on_touch, LV_EVENT_PRESSED, NULL);

    // Start the animation timer. MUST be deleted in cleanup().
    s_timer = lv_timer_create(@PREFIX@_tick, TIMER_MS, NULL);

    // TODO: make it yours. Ideas: bouncing shapes, particles (qt_random_u32),
    // a clock (qt_uptime_ms), pitch-reactive colors. See plugins/standby/bouncer
    // (a bouncing dot) or plugins/standby/hyperdrive (a warp starfield).
}

// Called when standby ends. DELETE THE TIMER HERE.
static void @PREFIX@_cleanup(void) {
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_label  = NULL;   // the host deletes screen children for us
    s_tinted = false;
}

// Called ~30x/second with the latest pitch (optional to use).
static void @PREFIX@_display_frequency(float frequency,
                                       float target_frequency,
                                       TunerNoteName note_name,
                                       int octave,
                                       float cents) {
    (void)frequency; (void)target_frequency; (void)octave; (void)cents;
    if (!s_label) { return; }

    // Tint the label while a note is being played.
    bool should_tint = (note_name != NOTE_NONE);
    if (should_tint != s_tinted) {
        lv_palette_t palette = qt_get_note_name_palette();
        lv_color_t colour = should_tint
            ? ((palette == LV_PALETTE_NONE) ? lv_palette_main(LV_PALETTE_AMBER)
                                            : lv_palette_main(palette))
            : lv_color_white();
        lv_obj_set_style_text_color(s_label, colour, 0);
        s_tinted = should_tint;
    }
}
