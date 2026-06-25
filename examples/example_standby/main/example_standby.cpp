// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/**
 * example_standby.cpp
 *
 * Minimal Q-Tune standby plugin demonstrating the TunerStandbyGUIInterface
 * contract.
 *
 * Animation: a small filled circle (dot) bounces around the screen using a
 * simple linear motion + wall-bounce model driven by an lv_timer. When the
 * device is in Buffered Bypass mode and a pitch is detected, display_frequency()
 * tints the dot to the user's accent colour; when no pitch is detected the dot
 * reverts to white.
 *
 * Orientation: works in BOTH portrait (240x320) and landscape (320x240). The
 * bounce walls are recomputed every tick from the live screen_width/screen_height
 * host globals (see es_timer_cb), so the animation fills whatever the current
 * orientation is — no portrait-specific assumptions.
 *
 * Build output: example_standby.so  (see CMakeLists.txt / qtune_project_so()).
 * Upload via the /plugins page served over Wi-Fi; the firmware loads it from
 * /data/plugins at boot.
 *
 * LANGUAGE: plugins are compiled as C++ (the interface headers pull in
 * tuner_math.h). The descriptor is kept in an extern "C" block so dlsym() finds
 * it by its plain name.
 */

#include "qtune_plugin.h"

#include <math.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static uint8_t      es_get_id(void);
static const char  *es_get_name(void);
static bool         es_enable_screen(void);
static void         es_init(lv_obj_t *screen);
static void         es_cleanup(void);
static void         es_display_frequency(float frequency,
                                          float target_frequency,
                                          TunerNoteName note_name,
                                          int octave,
                                          float cents);

// ---------------------------------------------------------------------------
// Interface struct
// ---------------------------------------------------------------------------
static TunerStandbyGUIInterface es_interface = {
    .get_id           = es_get_id,
    .get_name         = es_get_name,
    .enable_screen    = es_enable_screen,
    .init             = es_init,
    .cleanup          = es_cleanup,
    .display_frequency = es_display_frequency,
};

// ---------------------------------------------------------------------------
// Plugin descriptor — one per .so. extern "C" keeps the symbol unmangled;
// QTUNE_PLUGIN_EXPORT gives it default ELF visibility so dlsym() can find it.
// ---------------------------------------------------------------------------
extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_STANDBY,
    .sdk_build    = "example-sdk-1.0",
    .interface    = &es_interface,
};

// Entry function the firmware calls at load time. Required: the ELF loader's
// dlsym() finds only function symbols, not the descriptor data object above.
QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}

// ---------------------------------------------------------------------------
// Animation state
// ---------------------------------------------------------------------------
#define DOT_SIZE    24   // diameter of the bouncing dot in pixels
#define DOT_SPEED_X  3   // horizontal pixels per timer tick
#define DOT_SPEED_Y  2   // vertical pixels per timer tick
#define TIMER_MS    20   // timer period — 50 Hz

static lv_obj_t   *s_dot    = NULL;  // the bouncing circle
static lv_timer_t *s_timer  = NULL;  // lv_timer driving the animation

// Current position and velocity (integer pixels).
static int s_x  = 0;
static int s_y  = 0;
static int s_dx = DOT_SPEED_X;
static int s_dy = DOT_SPEED_Y;

// Current tint state — only update LVGL styles when this changes.
static bool s_tinted = false;

// ---------------------------------------------------------------------------
// Timer callback — advances the dot and handles wall bounces. The host holds the
// LVGL lock before calling lv_timer_handler(), so this runs inside the lock; do
// NOT call lvgl_port_lock() here.
// ---------------------------------------------------------------------------
static void es_timer_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_dot) { return; }

    s_x += s_dx;
    s_y += s_dy;

    // screen_width / screen_height are host globals updated by tuner_gui_task.
    int right  = (int)screen_width  - DOT_SIZE;
    int bottom = (int)screen_height - DOT_SIZE;

    if (s_x <= 0) {
        s_x  = 0;
        s_dx = DOT_SPEED_X;
    } else if (s_x >= right) {
        s_x  = right;
        s_dx = -DOT_SPEED_X;
    }

    if (s_y <= 0) {
        s_y  = 0;
        s_dy = DOT_SPEED_Y;
    } else if (s_y >= bottom) {
        s_y  = bottom;
        s_dy = -DOT_SPEED_Y;
    }

    lv_obj_set_pos(s_dot, (lv_coord_t)s_x, (lv_coord_t)s_y);
}

// ---------------------------------------------------------------------------
// Interface implementations
// ---------------------------------------------------------------------------

static uint8_t es_get_id(void) {
    // Must be in [QTUNE_PLUGIN_STANDBY_ID_MIN, QTUNE_PLUGIN_STANDBY_ID_MAX]
    // i.e. [210, 254].
    return 210;
}

static const char *es_get_name(void) {
    return "Dot";
}

static bool es_enable_screen(void) {
    // Return true to keep the display on during standby.
    // Return false for a "screen off" standby (like the built-in Blank UI).
    return true;
}

static void es_init(lv_obj_t *screen) {
    s_x      = DOT_SIZE;
    s_y      = DOT_SIZE;
    s_dx     = DOT_SPEED_X;
    s_dy     = DOT_SPEED_Y;
    s_tinted = false;

    // A filled circle: an lv_obj with a circular radius is the simplest solid
    // coloured shape in LVGL 9.
    s_dot = lv_obj_create(screen);
    lv_obj_set_size(s_dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);
    lv_obj_set_pos(s_dot, (lv_coord_t)s_x, (lv_coord_t)s_y);

    lv_obj_remove_flag(s_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_dot, LV_OBJ_FLAG_CLICKABLE);

    // The firmware holds the LVGL lock during init/cleanup/display_frequency —
    // do NOT call lvgl_port_lock()/unlock() anywhere in this plugin.
    s_timer = lv_timer_create(es_timer_cb, TIMER_MS, NULL);
}

static void es_cleanup(void) {
    // Stop and delete the timer BEFORE the host clears the screen, so it can't
    // fire against freed objects.
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_dot    = NULL;
    s_tinted = false;
}

static void es_display_frequency(float frequency,
                                  float target_frequency,
                                  TunerNoteName note_name,
                                  int octave,
                                  float cents) {
    (void)frequency;
    (void)target_frequency;
    (void)octave;
    (void)cents;

    if (!s_dot) { return; }

    // Tint the dot to the user's accent colour while a note is detected.
    bool should_tint = (note_name != NOTE_NONE);

    if (should_tint != s_tinted) {
        lv_palette_t palette = qt_get_note_name_palette();
        lv_color_t   colour;
        if (should_tint) {
            colour = (palette == LV_PALETTE_NONE)
                         ? lv_palette_main(LV_PALETTE_AMBER)
                         : lv_palette_main(palette);
        } else {
            colour = lv_color_white();
        }
        lv_obj_set_style_bg_color(s_dot, colour, 0);
        s_tinted = should_tint;
    }
}
