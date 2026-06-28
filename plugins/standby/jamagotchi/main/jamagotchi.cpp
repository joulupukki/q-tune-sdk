/*
 * jamagotchi.cpp — Jamagotchi (v3, need-driven)
 *
 * A musical virtual pet standby screen for the Q-Tune pedal. The pet hides its
 * meters and lives on screen; when a hidden need crosses a threshold it CALLS for
 * attention ("I'm hungry! Play E to feed me"). You answer by playing the prompted
 * note (buffered bypass) or tapping (no-guitar fallback), launching a short
 * full-screen mini-game whose score proportionally fills the need. Ignored calls
 * become care mistakes that shape evolution.
 *
 * Game model: pure-C, host-tested jama_core; mini-game math: jama_quiz. This file
 * is the LVGL UI + glue only. State persists via qt_state_* (committed sparingly;
 * never from display_frequency). Drawn from LVGL primitives; works in portrait and
 * landscape. Plugins can't delete individual objects, so one persistent root is
 * lv_obj_clean()'d to switch scenes; the single lv_timer is deleted in cleanup().
 */

#include "qtune_plugin.h"
#include "jama_core.h"
#include "jama_quiz.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Interface callbacks (forward declarations) ----------------------------
static const char  *jam_get_name(void);
static bool         jam_enable_screen(void);
static void         jam_init(lv_obj_t *screen);
static void         jam_cleanup(void);
static void         jam_display_frequency(float frequency, float target_frequency,
                                          TunerNoteName note_name, int octave, float cents);

static TunerStandbyGUIInterface jam_interface = {
    .get_name          = jam_get_name,
    .enable_screen     = jam_enable_screen,
    .init              = jam_init,
    .cleanup           = jam_cleanup,
    .display_frequency = jam_display_frequency,
};

extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version  = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type         = QTUNE_PLUGIN_STANDBY,
    .sdk_build    = "jamagotchi-3.1",
    .interface    = &jam_interface,
    .uid          = "qtune.jamagotchi.plxzk4",   // never change (keys saved data)
};
QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define JAM_STATE_KEY   "pet"
#define TICK_MS         50u      // 20 fps — smoother game animation
#define CROSS_LANES     4        // Road Dash: number of car lanes to cross
#define COMMIT_MS       30000u
#define PITCH_FRESH_MS  8000u
#define INPUT_LOCK_MS   600u
#define NOTE_HOLD_MS    90u      // brief hold to confirm a played note (snappy)
#define FEEDBACK_MS     260u     // flash a chosen button this long before advancing
#define JAM_TARGET_JUMPS 8       // Jam Run: obstacles to clear
#define BTN_BG          0x303030
#define BTN_BG_DIS      0x1A1A1A

static const char *NOTE_TXT[12] = {
    "A","A#","B","C","C#","D","D#","E","F","F#","G","G#"
};
enum { NC_A = 0, NC_B = 2, NC_D = 5, NC_E = 7, NC_G = 10 };

typedef enum { SC_PET = 0, SC_GAME, SC_RESULT, SC_MENU, SC_STATUS } jam_scene_t;
typedef enum { G_FEED = 0, G_JAM, G_CLEAN, G_HEAL, G_ECHO, G_COUNT } jam_game_t;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static jama_state_t s_pet;

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_root   = NULL;
static lv_timer_t *s_timer = NULL;

static jam_scene_t s_scene = SC_PET;
static bool     s_dirty = false;
static uint32_t s_last_commit_ms = 0;

// Pitch tracking.
static TunerNoteName s_prev_note = NOTE_NONE;
static bool          s_onset = false;
static uint32_t      s_last_note_ms = 0;
static uint32_t      s_note_start_ms = 0;

// Pitch-menu (call prompt / results / games menu).
#define MENU_MAX 5
static int       s_menu_n = 0;
static uint8_t   s_menu_note[MENU_MAX];
static int       s_menu_pick = -1;
static uint32_t  s_input_lock_until = 0;
static bool      s_saw_silence = false;

// Pet scene / call tracking.
static lv_obj_t *s_body = NULL, *s_eye_l = NULL, *s_eye_r = NULL, *s_mouth = NULL;
static lv_obj_t *s_speech = NULL, *s_hint = NULL;
static jama_call_t s_call = JAMA_CALL_NONE;
static uint8_t   s_call_note = NC_E;
static uint32_t  s_call_since_ms = 0;
static uint32_t  s_blink_until = 0, s_next_blink = 0;

// Active game.
static jam_game_t s_game = G_FEED;
static jama_call_t s_game_need = JAMA_CALL_NONE;
static int s_round = 0, s_rounds = 0, s_correct = 0, s_score = 0;
static uint8_t s_target = NC_A;
// Echo (Wordle-style): top row of sequence tiles that flip to reveal, then the
// player fills them by playing pad notes; a cursor marks the active slot.
#define ECHO_LEN     4
#define FLIP_FRAMES   6          // flip animation length (x TICK_MS)
#define ECHO_STEP_MS  650u       // reveal cadence
#define ECHO_END_PAUSE_MS 850u   // hold the full sequence before flipping back
enum { TILE_FACE = 0, TILE_REVEAL, TILE_FILL };
static uint8_t s_seq[ECHO_LEN]; static int s_seq_len = 0;
static uint8_t s_answer[ECHO_LEN];
static lv_obj_t *s_tile[ECHO_LEN]; static lv_obj_t *s_tile_lbl[ECHO_LEN];
static int s_tile_state[ECHO_LEN]; static uint8_t s_tile_note[ECHO_LEN];
static bool s_tile_wrong[ECHO_LEN];   // a filled answer tile that didn't match
static int s_flip_left[ECHO_LEN]; static int s_flip_to[ECHO_LEN]; static uint8_t s_flip_note[ECHO_LEN];
static int s_echo_phase = 0;     // 0=reveal, 1=flip-back, 2=input
static int s_echo_idx = 0, s_echo_cursor = 0; static uint32_t s_echo_t0 = 0;
static uint32_t s_echo_finish_at = 0;   // when to show the score (0 = not yet)
static uint32_t s_game_t0 = 0, s_game_deadline = 0;
static int s_py = 0, s_pvy = 0;
static int s_obst_x = 0; static int s_obst_passed = 0, s_obst_hit = 0;
static bool s_jam_running = false;   // Jam Run: false during the start countdown
static uint8_t s_spot_note[3]; static bool s_spot_done[3];   // Clean: note per speck
// Road Dash (Heal game): pet hops up across lanes of moving cars.
static lv_obj_t *s_car[CROSS_LANES];
static int s_car_x[CROSS_LANES], s_car_dir[CROSS_LANES], s_car_speed[CROSS_LANES];
static int s_cr_step = 0, s_cr_invuln = 0;
static bool s_busy = false; static uint32_t s_resume_at = 0;  // Feed feedback delay
static bool s_disabled[4] = { false, false, false, false };   // Echo pad
static lv_obj_t *s_g1 = NULL, *s_g2 = NULL, *s_g3 = NULL, *s_g4 = NULL;
static lv_obj_t *s_gnote = NULL, *s_gprog = NULL, *s_ginstr = NULL, *s_gbox_note = NULL;
static lv_obj_t *s_choice[4] = { NULL, NULL, NULL, NULL };
static uint8_t   s_choice_note[4];

static jama_call_t s_result_need = JAMA_CALL_NONE;
static uint32_t s_left_uptime_ms = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static lv_color_t accent_color(void) {
    lv_palette_t p = qt_get_note_name_palette();
    return (p == LV_PALETTE_NONE) ? lv_palette_main(LV_PALETTE_AMBER) : lv_palette_main(p);
}
static bool pitch_available(void) { return (qt_uptime_ms() - s_last_note_ms) < PITCH_FRESH_MS; }
static void persist_now(bool commit) {
    qt_state_set_blob(JAM_STATE_KEY, &s_pet, sizeof s_pet);
    if (commit && qt_state_commit() == QT_STATE_OK) { s_dirty = false; s_last_commit_ms = qt_uptime_ms(); }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *ud) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_bg_color(b, lv_color_hex(BTN_BG), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_pad_left(b, 14, 0);
    lv_obj_set_style_pad_right(b, 14, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return b;
}

static lv_obj_t *make_icon(lv_obj_t *parent, const char *glyph, lv_event_cb_t cb) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 60, 60);                 // large, easy tap target
    lv_obj_set_style_bg_color(b, lv_color_hex(0x282828), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_50, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
    lv_label_set_text(l, glyph);
    lv_obj_center(l);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return b;
}

// Wrapping, centered title at the top of a scene.
static lv_obj_t *make_title(const char *text) {
    lv_obj_t *t = lv_label_create(s_root);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(t, accent_color(), 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, lv_pct(96));
    lv_label_set_text(t, text);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    return t;
}

// A small centered instruction line near the bottom of a game.
static lv_obj_t *make_instr(const char *text) {
    lv_obj_t *l = lv_label_create(s_root);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(96));
    lv_label_set_text(l, text);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -6);
    return l;
}

// ---------------------------------------------------------------------------
// Scene switching
// ---------------------------------------------------------------------------
static void build_pet(void);
static void build_game(jam_game_t g, jama_call_t need);
static void build_result(jama_call_t need, int score);
static void build_menu(void);
static void build_status(void);

static void go_scene(jam_scene_t sc) {
    s_scene = sc;
    s_menu_n = 0; s_menu_pick = -1;
    s_input_lock_until = qt_uptime_ms() + INPUT_LOCK_MS;
    s_saw_silence = false;
    s_busy = false; s_resume_at = 0;
    s_body = s_eye_l = s_eye_r = s_mouth = s_speech = s_hint = NULL;
    s_gnote = s_gprog = s_ginstr = s_gbox_note = s_g1 = s_g2 = s_g3 = s_g4 = NULL;
    for (int i = 0; i < 4; i++) { s_choice[i] = NULL; s_disabled[i] = false; }
    for (int i = 0; i < CROSS_LANES; i++) s_car[i] = NULL;
    for (int i = 0; i < ECHO_LEN; i++) { s_tile[i] = NULL; s_tile_lbl[i] = NULL; s_flip_left[i] = 0; }
    lv_obj_clean(s_root);
}

static void menu_set(const uint8_t *notes, int n) {
    s_menu_n = n < MENU_MAX ? n : MENU_MAX;
    for (int i = 0; i < s_menu_n; i++) s_menu_note[i] = notes[i];
    // NOTE: do NOT clear s_menu_pick here. update_pet() re-arms the call menu
    // every tick; clearing the pick here would wipe a note the player just played
    // before pet_consume_pick() can act on it. go_scene() / the consumers reset it.
}

// ---------------------------------------------------------------------------
// PET scene
// ---------------------------------------------------------------------------
static void on_status_btn(lv_event_t *e) { (void)e; go_scene(SC_STATUS); build_status(); }
static void on_games_btn(lv_event_t *e)  { (void)e; go_scene(SC_MENU);   build_menu(); }
static void on_respond(lv_event_t *e);

static jam_game_t game_for_call(jama_call_t c) {
    switch (c) {
        case JAMA_CALL_HUNGRY:     return G_FEED;
        case JAMA_CALL_BORED:      return G_JAM;
        case JAMA_CALL_DIRTY:      return G_CLEAN;
        case JAMA_CALL_SICK:       return G_HEAL;
        case JAMA_CALL_DISCIPLINE: return G_ECHO;
        default:                   return G_JAM;
    }
}

static void build_pet(void) {
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);

    s_body = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, 84, 70);
    lv_obj_set_style_radius(s_body, 20, 0);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_COVER, 0);
    // Set the real colour now so it doesn't flash white before the first tick.
    lv_obj_set_style_bg_color(s_body,
        jama_is_dead(&s_pet) ? lv_color_hex(0x555555)
        : s_pet.sick ? lv_palette_darken(LV_PALETTE_GREEN, 2) : accent_color(), 0);
    lv_obj_set_style_border_width(s_body, 2, 0);
    lv_obj_set_style_border_color(s_body, lv_color_black(), 0);
    lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, is_landscape ? 14 : 34);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_body, on_respond, LV_EVENT_CLICKED, NULL);

    s_eye_l = lv_obj_create(s_body); lv_obj_remove_style_all(s_eye_l);
    lv_obj_set_size(s_eye_l, 10, 10); lv_obj_set_style_radius(s_eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_eye_l, lv_color_black(), 0); lv_obj_set_style_bg_opa(s_eye_l, LV_OPA_COVER, 0);
    lv_obj_align(s_eye_l, LV_ALIGN_CENTER, -18, -8); lv_obj_remove_flag(s_eye_l, LV_OBJ_FLAG_CLICKABLE);
    s_eye_r = lv_obj_create(s_body); lv_obj_remove_style_all(s_eye_r);
    lv_obj_set_size(s_eye_r, 10, 10); lv_obj_set_style_radius(s_eye_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_eye_r, lv_color_black(), 0); lv_obj_set_style_bg_opa(s_eye_r, LV_OPA_COVER, 0);
    lv_obj_align(s_eye_r, LV_ALIGN_CENTER, 18, -8); lv_obj_remove_flag(s_eye_r, LV_OBJ_FLAG_CLICKABLE);
    s_mouth = lv_obj_create(s_body); lv_obj_remove_style_all(s_mouth);
    lv_obj_set_size(s_mouth, 26, 5); lv_obj_set_style_radius(s_mouth, 2, 0);
    lv_obj_set_style_bg_color(s_mouth, lv_color_black(), 0); lv_obj_set_style_bg_opa(s_mouth, LV_OPA_COVER, 0);
    lv_obj_align(s_mouth, LV_ALIGN_CENTER, 0, 16); lv_obj_remove_flag(s_mouth, LV_OBJ_FLAG_CLICKABLE);

    s_speech = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_speech, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_speech, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_speech, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_speech, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_speech, lv_pct(90));
    lv_obj_align(s_speech, LV_ALIGN_TOP_MID, 0, is_landscape ? 92 : 116);
    lv_label_set_text(s_speech, "");

    s_hint = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_hint, lv_pct(70));
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -66);   // above the 60px corner icons
    lv_label_set_text(s_hint, "");

    lv_obj_t *st = make_icon(s_root, "i", on_status_btn);
    lv_obj_align(st, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_obj_t *gm = make_icon(s_root, LV_SYMBOL_PLAY, on_games_btn);
    lv_obj_align(gm, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
}

static void on_respond(lv_event_t *e) {
    (void)e;
    if (jama_is_dead(&s_pet)) { jama_init(&s_pet); s_dirty = true; go_scene(SC_PET); build_pet(); return; }
    if (s_call != JAMA_CALL_NONE) { go_scene(SC_GAME); build_game(game_for_call(s_call), s_call); }
}

static void update_pet(void) {
    uint32_t now = qt_uptime_ms();
    bool dead = jama_is_dead(&s_pet);

    jama_call_t pending = jama_pending_call(&s_pet);
    if (pending != s_call) {
        s_call = pending; s_call_since_ms = now;
        if (s_call != JAMA_CALL_NONE) s_call_note = (uint8_t)(qt_random_u32() % 12);
    }
    if (s_call != JAMA_CALL_NONE && (now - s_call_since_ms) > JAMA_CALL_IGNORE_SEC * 1000u) {
        jama_register_care_miss(&s_pet); s_dirty = true; s_call_since_ms = now;
    }

    bool calling = (s_call != JAMA_CALL_NONE) && !dead && !s_pet.sleeping;
    if (s_speech) {
        if (dead) lv_label_set_text(s_speech, "Your pet has passed on.\nTap to start a new egg.");
        else if (s_pet.sleeping) lv_label_set_text(s_speech, "Zzz...");
        else if (calling) {
            // Always offer BOTH ways to respond: play the note, or tap.
            char m[64];
            snprintf(m, sizeof m, "I'm %s!\nPlay %s  (or tap me)", jama_call_word(s_call), NOTE_TXT[s_call_note]);
            lv_label_set_text(s_speech, m);
        } else lv_label_set_text(s_speech, jama_stage_name(s_pet.stage));
    }
    if (s_hint) {
        // Only nudge about Buffered Bypass when we haven't actually heard the guitar.
        lv_label_set_text(s_hint, (calling && !pitch_available())
            ? "Tip: enable Buffered Bypass" : "");
    }

    // Always arm the play-the-note path while calling (harmless if no signal).
    if (calling) menu_set(&s_call_note, 1);
    else s_menu_n = 0;

    if (s_body) {
        lv_color_t col = accent_color();
        if (dead) col = lv_color_hex(0x555555);
        else if (s_pet.sick) col = lv_palette_darken(LV_PALETTE_GREEN, 2);
        lv_obj_set_style_bg_color(s_body, col, 0);
        bool blink = now < s_blink_until;
        if (now >= s_next_blink) { s_blink_until = now + 120; s_next_blink = now + 2000 + (qt_random_u32() % 2500); }
        bool hide_eyes = blink || s_pet.sleeping || dead;
        if (s_eye_l) { if (hide_eyes) lv_obj_add_flag(s_eye_l, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(s_eye_l, LV_OBJ_FLAG_HIDDEN); }
        if (s_eye_r) { if (hide_eyes) lv_obj_add_flag(s_eye_r, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(s_eye_r, LV_OBJ_FLAG_HIDDEN); }
        if (s_mouth) lv_obj_set_width(s_mouth, 14 + s_pet.happiness / 6);
        int base = is_landscape ? 14 : 34;
        int dy = (!dead && !s_pet.sleeping) ? (int)roundf(3.0f * sinf((float)now / 260.0f)) : 0;
        lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, base + dy);
    }
}

static void pet_consume_pick(void) { if (s_menu_pick >= 0) { s_menu_pick = -1; on_respond(NULL); } }

// ---------------------------------------------------------------------------
// MENU scene — 2-column (or 3 in landscape) grid of square tiles.
// ---------------------------------------------------------------------------
static void on_menu_pick(lv_event_t *e) { go_scene(SC_GAME); build_game((jam_game_t)(intptr_t)lv_event_get_user_data(e), JAMA_CALL_NONE); }
static void on_menu_back(lv_event_t *e) { (void)e; go_scene(SC_PET); build_pet(); }

static lv_obj_t *make_tile(const char *name, const char *note, lv_event_cb_t cb, void *ud,
                           int x, int y, int w, int h) {
    lv_obj_t *b = lv_obj_create(s_root);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(BTN_BG), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *nm = lv_label_create(b);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(nm, lv_color_white(), 0);
    lv_label_set_text(nm, name);
    lv_obj_align(nm, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_remove_flag(nm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *nt = lv_label_create(b);
    lv_obj_set_style_text_font(nt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(nt, accent_color(), 0);
    lv_label_set_text(nt, note);
    lv_obj_align(nt, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_remove_flag(nt, LV_OBJ_FLAG_CLICKABLE);
    return b;
}

static void build_menu(void) {
    static const char *NAMES[G_COUNT] = { "Feed", "Jam", "Clean", "Heal", "Echo" };
    static const uint8_t NOTES[G_COUNT] = { NC_E, NC_A, NC_D, NC_G, NC_B };

    make_title("Games  (play a string)");

    int W = (int)screen_width, H = (int)screen_height;
    int cols = is_landscape ? 3 : 2;
    int gap = 8;
    int top = 36, botrsv = 46;                  // reserve for title + Back
    int rows = (G_COUNT + cols - 1) / cols;
    int w = (W - (cols + 1) * gap) / cols;
    int h = (H - top - botrsv - (rows + 1) * gap) / rows;
    if (h > 78) h = 78; if (h < 44) h = 44;
    for (int i = 0; i < G_COUNT; i++) {
        int r = i / cols, c = i % cols;
        char nt[8]; snprintf(nt, sizeof nt, "(%s)", NOTE_TXT[NOTES[i]]);
        int x = gap + c * (w + gap);
        int y = top + gap + r * (h + gap);
        make_tile(NAMES[i], nt, on_menu_pick, (void *)(intptr_t)i, x, y, w, h);
    }
    lv_obj_t *back = make_button(s_root, "Back", on_menu_back, NULL);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -4);

    uint8_t notes[G_COUNT]; for (int i = 0; i < G_COUNT; i++) notes[i] = NOTES[i];
    menu_set(notes, G_COUNT);
}
static void menu_consume_pick(void) {
    if (s_menu_pick < 0) return;
    int g = s_menu_pick; s_menu_pick = -1;
    go_scene(SC_GAME); build_game((jam_game_t)g, JAMA_CALL_NONE);
}

// ---------------------------------------------------------------------------
// STATUS scene — progress bars.
// ---------------------------------------------------------------------------
static void on_status_back(lv_event_t *e) { (void)e; go_scene(SC_PET); build_pet(); }

static void status_bar(const char *label, int value, int y) {
    lv_obj_t *l = lv_label_create(s_root);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
    lv_label_set_text(l, label);
    lv_obj_set_pos(l, 12, y);
    lv_obj_t *track = lv_obj_create(s_root);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, lv_pct(58), 14);
    lv_obj_align(track, LV_ALIGN_TOP_RIGHT, -12, y);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x222222), 0); lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(track, 4, 0);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *fill = lv_obj_create(track);
    lv_obj_remove_style_all(fill);
    int v = value < 0 ? 0 : (value > 100 ? 100 : value);
    lv_obj_set_size(fill, lv_pct(v), lv_pct(100));
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_color_t c = (v >= 50) ? lv_palette_main(LV_PALETTE_GREEN)
                 : (v >= 25) ? lv_palette_main(LV_PALETTE_AMBER) : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_bg_color(fill, c, 0); lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(fill, 4, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
}

static void build_status(void) {
    char t[40]; snprintf(t, sizeof t, "%s", jama_stage_name(s_pet.stage));
    make_title(t);
    int y = is_landscape ? 38 : 50, step = is_landscape ? 30 : 38;
    status_bar("Hunger", s_pet.hunger, y);     y += step;
    status_bar("Happy",  s_pet.happiness, y);  y += step;
    status_bar("Focus",  s_pet.discipline, y); y += step;   // discipline = focus/training
    status_bar("Energy", s_pet.energy, y);     y += step;
    status_bar("Health", s_pet.health, y);
    lv_obj_t *back = make_button(s_root, "Back", on_status_back, NULL);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -4);
}

// ---------------------------------------------------------------------------
// RESULT scene.
// ---------------------------------------------------------------------------
static void on_result_retry(lv_event_t *e) { (void)e; go_scene(SC_GAME); build_game(s_game, s_result_need); }
static void on_result_cont(lv_event_t *e)  { (void)e; go_scene(SC_PET);  build_pet(); }

static void build_result(jama_call_t need, int score) {
    s_result_need = need;
    lv_obj_t *t = lv_label_create(s_root);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(t, accent_color(), 0);
    char m[16]; snprintf(m, sizeof m, "%d%%", score);
    lv_label_set_text(t, m);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_t *sub = lv_label_create(s_root);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sub, lv_color_white(), 0);
    lv_label_set_text(sub, score >= JAMA_CLEAR_THRESHOLD ? "Nice!" : "Good try!");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 76);

    // Two square, side-by-side tiles: label on top, note to play on the bottom.
    int W = (int)screen_width, H = (int)screen_height, gap = 12;
    int w = (W - 3 * gap) / 2; if (w > 130) w = 130;
    int h = w; if (h > 96) h = 96;
    int gx = (W - (w * 2 + gap)) / 2;
    int gy = H - h - 14;
    make_tile("Try Again", "(A)", on_result_retry, NULL, gx, gy, w, h);
    make_tile("Continue",  "(E)", on_result_cont,  NULL, gx + w + gap, gy, w, h);
    static const uint8_t notes[2] = { NC_A, NC_E };
    menu_set(notes, 2);
}
static void result_consume_pick(void) {
    if (s_menu_pick < 0) return;
    int p = s_menu_pick; s_menu_pick = -1;
    if (p == 0) { go_scene(SC_GAME); build_game(s_game, s_result_need); }
    else        { go_scene(SC_PET);  build_pet(); }
}

// ---------------------------------------------------------------------------
// GAMES
// ---------------------------------------------------------------------------
static void game_finish(void) {
    int score = s_score; if (score < 0) score = 0; if (score > 100) score = 100;
    if (s_game_need != JAMA_CALL_NONE) jama_satisfy_call(&s_pet, s_game_need, (uint8_t)score);
    else                               jama_freeplay_reward(&s_pet, (uint8_t)score);
    s_dirty = true; s_call = JAMA_CALL_NONE;
    go_scene(SC_RESULT); build_result(s_game_need, score);
}

static void on_choice(lv_event_t *e);
static void on_spot(lv_event_t *e);
static void clean_spot(int i);

// Lay the 4 choice buttons in a big 2x2 grid bottom-anchored; return grid top y.
static int place_choice_grid(void) {
    int W = (int)screen_width, H = (int)screen_height, gap = 12;
    int bw = (W - 3 * gap) / 2; if (bw > 130) bw = 130;
    int bh = is_landscape ? 46 : 60;
    int gw = bw * 2 + gap, gx = (W - gw) / 2;
    int gy = H - (bh * 2 + gap) - 42;   // leave room for the instruction line
    int pos[4][2] = { {0,0},{1,0},{0,1},{1,1} };
    for (int i = 0; i < 4; i++) if (s_choice[i]) {
        lv_obj_set_size(s_choice[i], bw, bh);
        lv_obj_set_pos(s_choice[i], gx + pos[i][0] * (bw + gap), gy + pos[i][1] * (bh + gap));
    }
    return gy;
}

static void choice_set_label(int i, const char *txt) {
    if (!s_choice[i]) return;
    lv_obj_t *lab = lv_obj_get_child(s_choice[i], 0);
    if (lab) lv_label_set_text(lab, txt);
}
static void choice_set_bg(int i, uint32_t hex) {
    if (s_choice[i]) lv_obj_set_style_bg_color(s_choice[i], lv_color_hex(hex), 0);
}

static void build_choices(void) {
    for (int i = 0; i < 4; i++) {
        s_choice[i] = make_button(s_root, "?", on_choice, (void *)(intptr_t)i);
        lv_obj_t *lab = lv_obj_get_child(s_choice[i], 0);
        if (lab) lv_obj_set_style_text_font(lab, &lv_font_montserrat_40, 0);   // big, bold-looking
    }
    place_choice_grid();
}

static void feed_new_target(void) {
    uint8_t distract[3];
    s_target = (uint8_t)(qt_random_u32() % 12);
    jama_pick_distinct(distract, 3, s_target, qt_random_u32);
    int slot = qt_random_u32() % 4, d = 0;
    for (int i = 0; i < 4; i++) { s_choice_note[i] = (i == slot) ? s_target : distract[d++]; }
    for (int i = 0; i < 4; i++) { choice_set_label(i, NOTE_TXT[s_choice_note[i]]); choice_set_bg(i, BTN_BG); }
    if (s_gnote) lv_label_set_text(s_gnote, NOTE_TXT[s_target]);
    if (s_gprog) { char p[16]; snprintf(p, sizeof p, "%d/%d", s_round + 1, s_rounds); lv_label_set_text(s_gprog, p); }
}

static lv_obj_t *game_progress(void) {
    lv_obj_t *p = lv_label_create(s_root);
    lv_obj_set_style_text_font(p, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(p, lv_color_hex(0x999999), 0);
    lv_label_set_text(p, "");
    lv_obj_align(p, LV_ALIGN_TOP_RIGHT, -8, 8);
    return p;
}

// --- Echo sequence tiles (Wordle-style) ------------------------------------
static void echo_tile_geom(int *tw, int *y) {
    int W = (int)screen_width, gap = 8;
    int t = (W - (ECHO_LEN + 1) * gap) / ECHO_LEN; if (t > 48) t = 48;
    *tw = t; *y = is_landscape ? 40 : 58;
}
// Render tile i from its state, the in-progress flip width, and the cursor.
static void echo_tile_apply(int i) {
    if (!s_tile[i]) return;
    int tw, y; echo_tile_geom(&tw, &y);
    int gap = 8, total = ECHO_LEN * tw + (ECHO_LEN - 1) * gap;
    int x0 = ((int)screen_width - total) / 2;
    int cx = x0 + i * (tw + gap) + tw / 2;
    int curw = tw;
    if (s_flip_left[i] > 0) {                         // squash width to fake a flip
        float p = (float)(FLIP_FRAMES - s_flip_left[i]) / (float)FLIP_FRAMES;
        float f = fabsf(2.0f * p - 1.0f);            // 1 -> 0 -> 1
        curw = (int)(tw * f); if (curw < 2) curw = 2;
    }
    lv_obj_set_size(s_tile[i], curw, tw);
    lv_obj_set_pos(s_tile[i], cx - curw / 2, y);
    int st = s_tile_state[i];
    bool cursor = (s_echo_phase == 2 && i == s_echo_cursor);
    lv_color_t bg, border, fg = lv_color_white(); int bw; const char *txt = "";
    if (st == TILE_FILL) {                            // your guess: red if wrong, accent if right
        bg = s_tile_wrong[i] ? lv_palette_main(LV_PALETTE_RED) : accent_color();
        border = bg; bw = 2; txt = NOTE_TXT[s_tile_note[i]];
        fg = s_tile_wrong[i] ? lv_color_white() : lv_color_black();
    } else if (st == TILE_REVEAL) { bg = lv_color_hex(0x303030); border = accent_color(); bw = 2; txt = NOTE_TXT[s_tile_note[i]]; }
    else                          { bg = lv_color_hex(0x1c1c1c); border = lv_color_hex(0x555555); bw = 2; }
    if (cursor) { border = lv_color_white(); bw = 3; }
    lv_obj_set_style_bg_color(s_tile[i], bg, 0);
    lv_obj_set_style_border_color(s_tile[i], border, 0);
    lv_obj_set_style_border_width(s_tile[i], bw, 0);
    if (s_tile_lbl[i]) {
        lv_label_set_text(s_tile_lbl[i], (curw > tw / 2) ? txt : "");   // hide text mid-flip
        lv_obj_set_style_text_color(s_tile_lbl[i], fg, 0);
    }
}
static void echo_start_flip(int i, int to_state, uint8_t note) {
    s_flip_left[i] = FLIP_FRAMES; s_flip_to[i] = to_state; s_flip_note[i] = note;
}
// Record a played/tapped pad note into the current cursor slot.
static void game_finish(void);
static void echo_input(uint8_t note) {
    if (s_echo_phase != 2 || s_busy || s_echo_cursor >= s_seq_len) return;
    s_answer[s_echo_cursor] = note;
    s_tile_wrong[s_echo_cursor] = !jama_note_matches(note, s_seq[s_echo_cursor]);  // red if wrong
    echo_start_flip(s_echo_cursor, TILE_FILL, note);
    s_echo_cursor++;
    if (s_gprog) { char p[8]; snprintf(p, sizeof p, "%d/%d", s_echo_cursor, s_seq_len); lv_label_set_text(s_gprog, p); }
    if (s_echo_cursor >= s_seq_len) {
        int c = 0; for (int i = 0; i < s_seq_len; i++) if (jama_note_matches(s_answer[i], s_seq[i])) c++;
        s_correct = c; s_score = c * 100 / s_seq_len;
        // Don't use s_busy here (it would freeze the tile flips). Finish after the
        // last flip plays + a short pause; the Echo update keeps animating.
        s_echo_finish_at = qt_uptime_ms() + FLIP_FRAMES * TICK_MS + 600;
    }
}

static void build_game(jam_game_t g, jama_call_t need) {
    s_game = g; s_game_need = need;
    s_round = 0; s_correct = 0; s_score = 0;
    s_busy = false; s_resume_at = 0; s_note_start_ms = 0;
    s_game_t0 = qt_uptime_ms();

    switch (g) {
        case G_FEED: {
            s_rounds = 6;
            make_title("Feed time!\nPlay the note");
            s_gprog = game_progress();
            s_gnote = lv_label_create(s_root);
            lv_obj_set_style_text_font(s_gnote, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(s_gnote, lv_color_white(), 0);
            lv_obj_align(s_gnote, LV_ALIGN_TOP_MID, 0, is_landscape ? 36 : 64);
            build_choices();
            feed_new_target();
            make_instr("Play the note shown - or tap it");
        } break;
        case G_ECHO: {
            s_seq_len = ECHO_LEN; s_correct = 0;
            jama_pick_distinct(s_seq, s_seq_len, 12, qt_random_u32);   // distinct sequence
            make_title("Echo!\nWatch, then repeat");
            s_gprog = game_progress();
            // Top row of sequence tiles (start face-down).
            int tw, ty; echo_tile_geom(&tw, &ty);
            for (int i = 0; i < ECHO_LEN; i++) {
                s_tile[i] = lv_obj_create(s_root); lv_obj_remove_style_all(s_tile[i]);
                lv_obj_set_style_bg_opa(s_tile[i], LV_OPA_COVER, 0);
                lv_obj_set_style_border_opa(s_tile[i], LV_OPA_COVER, 0);
                lv_obj_set_style_radius(s_tile[i], 6, 0);
                lv_obj_remove_flag(s_tile[i], LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_remove_flag(s_tile[i], LV_OBJ_FLAG_CLICKABLE);
                s_tile_lbl[i] = lv_label_create(s_tile[i]);
                lv_obj_set_style_text_font(s_tile_lbl[i], &lv_font_montserrat_28, 0);
                lv_obj_center(s_tile_lbl[i]); lv_obj_remove_flag(s_tile_lbl[i], LV_OBJ_FLAG_CLICKABLE);
                s_tile_state[i] = TILE_FACE; s_flip_left[i] = 0; s_tile_wrong[i] = false;
                echo_tile_apply(i);
            }
            // 2x2 pad: the 4 sequence notes (distinct), shuffled. ALWAYS active —
            // never coloured or disabled; the player may play a note more than once.
            build_choices();
            uint8_t order[4] = {0, 1, 2, 3};
            for (int i = 3; i > 0; i--) { int j = qt_random_u32() % (i + 1); int t = order[i]; order[i] = order[j]; order[j] = t; }
            for (int i = 0; i < 4; i++) { s_choice_note[i] = s_seq[order[i]]; choice_set_label(i, NOTE_TXT[s_choice_note[i]]); }
            s_echo_phase = 0; s_echo_idx = 0; s_echo_cursor = 0; s_echo_t0 = qt_uptime_ms();
            s_echo_finish_at = 0;
            make_instr("Watch, then play them in order");
        } break;
        case G_JAM: {
            make_title("Jam Run!\nPlay the box's note (or tap) to jump");
            s_gprog = game_progress();
            int H = (int)screen_height, ground = H / 4;
            s_g1 = lv_obj_create(s_root); lv_obj_remove_style_all(s_g1);
            lv_obj_set_size(s_g1, lv_pct(100), 3);
            lv_obj_set_style_bg_color(s_g1, lv_color_hex(0x444444), 0); lv_obj_set_style_bg_opa(s_g1, LV_OPA_COVER, 0);
            lv_obj_align(s_g1, LV_ALIGN_CENTER, 0, ground);
            lv_obj_remove_flag(s_g1, LV_OBJ_FLAG_CLICKABLE);
            s_g2 = lv_obj_create(s_root); lv_obj_remove_style_all(s_g2);    // pet (with note)
            lv_obj_set_size(s_g2, 60, 60); lv_obj_set_style_radius(s_g2, 10, 0);
            lv_obj_set_style_bg_color(s_g2, accent_color(), 0); lv_obj_set_style_bg_opa(s_g2, LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_g2, LV_OBJ_FLAG_CLICKABLE);
            s_gbox_note = lv_label_create(s_g2);                            // big, bold-ish note
            lv_obj_set_style_text_font(s_gbox_note, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(s_gbox_note, lv_color_black(), 0);
            lv_obj_center(s_gbox_note); lv_obj_remove_flag(s_gbox_note, LV_OBJ_FLAG_CLICKABLE);
            s_g3 = lv_obj_create(s_root); lv_obj_remove_style_all(s_g3);    // obstacle
            lv_obj_set_size(s_g3, 20, 30); lv_obj_set_style_bg_color(s_g3, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_bg_opa(s_g3, LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_g3, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(s_g3, LV_OBJ_FLAG_HIDDEN);                      // hidden during countdown
            // Big centred countdown label (Get Ready! / 3 / 2 / 1 / Go!).
            s_gnote = lv_label_create(s_root);
            lv_obj_set_style_text_font(s_gnote, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(s_gnote, lv_color_white(), 0);
            lv_obj_set_style_text_align(s_gnote, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(s_gnote, LV_ALIGN_CENTER, 0, is_landscape ? -30 : -50);  // upper area, clear of the box
            lv_label_set_text(s_gnote, "Get Ready!");
            s_py = 0; s_pvy = 0; s_obst_x = (int)screen_width; s_obst_passed = 0; s_obst_hit = 0;
            s_jam_running = false;                 // start in countdown; s_game_t0 set above
            s_target = (uint8_t)(qt_random_u32() % 12);
            lv_label_set_text(s_gbox_note, NOTE_TXT[s_target]);
            if (s_gprog) { char p[16]; snprintf(p, sizeof p, "0/%d", JAM_TARGET_JUMPS); lv_label_set_text(s_gprog, p); }
        } break;
        case G_CLEAN: {
            make_title("Tidy up!\nClear each speck");
            s_gprog = game_progress();
            // Three specks, each labelled with a note: play it (or tap the speck)
            // to clear it. Reliable note-name matching — no pitch steering.
            jama_pick_distinct(s_spot_note, 3, 12, qt_random_u32);
            int W = (int)screen_width;
            lv_obj_t **spots[3] = { &s_g2, &s_g3, &s_g4 };
            for (int i = 0; i < 3; i++) {
                s_spot_done[i] = false;
                lv_obj_t *sp = lv_obj_create(s_root); lv_obj_remove_style_all(sp);
                lv_obj_set_size(sp, 64, 64); lv_obj_set_style_radius(sp, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_bg_color(sp, lv_palette_main(LV_PALETTE_BROWN), 0);
                lv_obj_set_style_bg_opa(sp, LV_OPA_COVER, 0);
                lv_obj_remove_flag(sp, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_add_flag(sp, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(sp, on_spot, LV_EVENT_CLICKED, (void *)(intptr_t)i);
                lv_obj_align(sp, LV_ALIGN_CENTER, (i - 1) * (W / 3), is_landscape ? 4 : 10);
                lv_obj_t *lab = lv_label_create(sp);
                lv_obj_set_style_text_font(lab, &lv_font_montserrat_28, 0);
                lv_obj_set_style_text_color(lab, lv_color_white(), 0);
                lv_label_set_text(lab, NOTE_TXT[s_spot_note[i]]);
                lv_obj_center(lab); lv_obj_remove_flag(lab, LV_OBJ_FLAG_CLICKABLE);
                *spots[i] = sp;
            }
            s_correct = 0;
            s_game_deadline = qt_uptime_ms() + 20000;
            if (s_gprog) lv_label_set_text(s_gprog, "0/3");
            make_instr("Play each speck's note - or tap it");
        } break;
        case G_HEAL: {   // "Road Dash" — Crossy-Road-style hop across traffic
            make_title("Road Dash!\nHop with a note or tap");
            s_gprog = game_progress();
            s_cr_step = 0; s_cr_invuln = 0;
            int W = (int)screen_width;
            s_g1 = lv_obj_create(s_root); lv_obj_remove_style_all(s_g1);    // pet (hopper)
            lv_obj_set_size(s_g1, 26, 26); lv_obj_set_style_radius(s_g1, 6, 0);
            lv_obj_set_style_bg_color(s_g1, accent_color(), 0); lv_obj_set_style_bg_opa(s_g1, LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_g1, LV_OBJ_FLAG_CLICKABLE);
            for (int i = 0; i < CROSS_LANES; i++) {
                s_car[i] = lv_obj_create(s_root); lv_obj_remove_style_all(s_car[i]);
                lv_obj_set_size(s_car[i], 44, 22); lv_obj_set_style_radius(s_car[i], 4, 0);
                lv_obj_set_style_bg_color(s_car[i], lv_palette_main((i & 1) ? LV_PALETTE_RED : LV_PALETTE_ORANGE), 0);
                lv_obj_set_style_bg_opa(s_car[i], LV_OPA_COVER, 0);
                lv_obj_remove_flag(s_car[i], LV_OBJ_FLAG_CLICKABLE);
                s_car_x[i] = (int)(qt_random_u32() % (uint32_t)W);
                s_car_dir[i] = (i & 1) ? 1 : -1;
                s_car_speed[i] = 4 + (int)(qt_random_u32() % 4);   // 4..7 px/tick
            }
            s_game_deadline = qt_uptime_ms() + 30000;
            if (s_gprog) { char p[8]; snprintf(p, sizeof p, "0/%d", CROSS_LANES); lv_label_set_text(s_gprog, p); }
            make_instr("Hop up when the lane is clear");
        } break;
        default: break;
    }
}

// Apply a Feed answer (after the feedback flash).
static void feed_advance(void) {
    s_round++;
    if (s_round >= s_rounds) { s_score = s_correct * 100 / s_rounds; game_finish(); }
    else feed_new_target();
}

// Choose tile i in Feed (by tap or by playing its note): score, flash, advance.
static void feed_pick(int i) {
    if (i < 0 || i > 3) return;
    bool ok = jama_note_matches(s_choice_note[i], s_target);
    if (ok) s_correct++;
    choice_set_bg(i, ok ? 0x2E7D32 : 0x9A1F1F);   // green / red
    s_busy = true; s_resume_at = qt_uptime_ms() + FEEDBACK_MS;
}

static void on_choice(lv_event_t *e) {
    if (s_scene != SC_GAME || s_busy) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_game == G_FEED) {
        feed_pick(i);
    } else if (s_game == G_ECHO) {
        echo_input(s_choice_note[i]);   // pad tiles stay active; fill the cursor slot
    }
}

// Clear one Clean speck (by tap or by playing its note).
static void clean_spot(int i) {
    if (i < 0 || i > 2 || s_spot_done[i]) return;
    lv_obj_t *sp = (i == 0) ? s_g2 : (i == 1) ? s_g3 : s_g4;
    s_spot_done[i] = true;
    if (sp) lv_obj_add_flag(sp, LV_OBJ_FLAG_HIDDEN);
    s_correct++;
    if (s_gprog) { char p[8]; snprintf(p, sizeof p, "%d/3", s_correct); lv_label_set_text(s_gprog, p); }
    if (s_correct >= 3) { s_score = 100; game_finish(); }
}
static void on_spot(lv_event_t *e) {
    if (s_scene != SC_GAME || s_game != G_CLEAN) return;
    clean_spot((int)(intptr_t)lv_event_get_user_data(e));
}

static void game_on_pitch(TunerNoteName note, int octave, float cents, bool onset) {
    if (s_scene != SC_GAME || s_busy) return;
    (void)octave; (void)cents;
    uint32_t now = qt_uptime_ms();
    switch (s_game) {
        case G_FEED: {
            // Only a note that matches one of the 4 tiles counts (right or wrong);
            // notes not on any tile are ignored and never advance. Match by note
            // name only (no in-tune requirement); a brief hold avoids stray hits.
            if (note == NOTE_NONE) { s_note_start_ms = 0; return; }
            int hit = -1;
            for (int i = 0; i < 4; i++) if (jama_note_matches(s_choice_note[i], (int)note)) { hit = i; break; }
            if (hit < 0) { s_note_start_ms = 0; return; }
            if (s_note_start_ms == 0) s_note_start_ms = now;
            if (now - s_note_start_ms >= NOTE_HOLD_MS) { s_note_start_ms = 0; feed_pick(hit); }
        } break;
        case G_ECHO:
            // Only notes shown in the 2x2 pad are interpreted; a fresh attack of
            // such a note fills the current slot.
            if (s_echo_phase == 2 && onset && note != NOTE_NONE) {
                for (int i = 0; i < 4; i++)
                    if (jama_note_matches(s_choice_note[i], (int)note)) { echo_input(s_choice_note[i]); break; }
            }
            break;
        case G_JAM:
            if (s_jam_running && onset && note != NOTE_NONE && s_py == 0 && jama_note_matches((int)note, s_target)) s_pvy = -13;
            break;
        case G_CLEAN:
            // Play a speck's note (fresh attack) to clear that speck.
            if (onset && note != NOTE_NONE) {
                for (int i = 0; i < 3; i++)
                    if (!s_spot_done[i] && jama_note_matches((int)note, s_spot_note[i])) { clean_spot(i); break; }
            }
            break;
        case G_HEAL:   // Road Dash: any fresh note hops the pet up one lane
            if (onset && note != NOTE_NONE && s_cr_step <= CROSS_LANES) s_cr_step++;
            break;
        default: break;
    }
}

static void game_update(void) {
    uint32_t now = qt_uptime_ms();
    if (s_busy) {
        if (now < s_resume_at) return;
        s_busy = false;
        if (s_game == G_FEED) { feed_advance(); return; }
        return;   // (Echo finishes via s_echo_finish_at so its flips keep animating)
    }
    switch (s_game) {
        case G_ECHO: {
            // Animate any in-progress flips (swap face at the midpoint), then run
            // the phase machine: reveal one-by-one -> flip all back -> input.
            for (int i = 0; i < ECHO_LEN; i++) {
                if (s_flip_left[i] > 0) {
                    int half = FLIP_FRAMES / 2, before = FLIP_FRAMES - s_flip_left[i];
                    s_flip_left[i]--;
                    int after = FLIP_FRAMES - s_flip_left[i];
                    if (before < half && after >= half) { s_tile_state[i] = s_flip_to[i]; s_tile_note[i] = s_flip_note[i]; }
                }
                echo_tile_apply(i);   // also refreshes the cursor highlight
            }
            bool any_flip = false; for (int i = 0; i < ECHO_LEN; i++) if (s_flip_left[i] > 0) any_flip = true;
            if (s_echo_phase == 0) {                      // reveal
                if (!any_flip) {
                    if (s_echo_idx < s_seq_len) {
                        if (now - s_echo_t0 >= ECHO_STEP_MS) { echo_start_flip(s_echo_idx, TILE_REVEAL, s_seq[s_echo_idx]); s_echo_idx++; s_echo_t0 = now; }
                    } else if (now - s_echo_t0 >= ECHO_END_PAUSE_MS) {   // hold full sequence, then flip back
                        s_echo_phase = 1; s_echo_t0 = now;
                        for (int i = 0; i < ECHO_LEN; i++) echo_start_flip(i, TILE_FACE, 0);
                    }
                }
            } else if (s_echo_phase == 1) {               // flip back to face-down
                if (!any_flip) { s_echo_phase = 2; s_echo_cursor = 0; }
            }
            // All slots filled: let the last flip play, pause, then show the score.
            if (s_echo_finish_at && now >= s_echo_finish_at) { s_echo_finish_at = 0; game_finish(); }
        } break;
        case G_JAM: {
            int H = (int)screen_height, ground = H / 4;
            int petx = (int)screen_width / 4;
            if (!s_jam_running) {
                // Countdown: the note is already visible in the box; give the player
                // time to prepare, then start.
                uint32_t e = now - s_game_t0;
                const char *t;
                if      (e < 1200) t = "Get Ready!";
                else if (e < 1900) t = "3";
                else if (e < 2600) t = "2";
                else if (e < 3300) t = "1";
                else if (e < 3800) t = "Go!";
                else {
                    s_jam_running = true; s_game_t0 = now; s_obst_x = (int)screen_width;
                    if (s_gnote) lv_obj_add_flag(s_gnote, LV_OBJ_FLAG_HIDDEN);
                    if (s_g3) lv_obj_remove_flag(s_g3, LV_OBJ_FLAG_HIDDEN);
                    t = "";
                }
                if (s_gnote) lv_label_set_text(s_gnote, t);
                if (s_g2) lv_obj_align(s_g2, LV_ALIGN_CENTER, -petx, ground - 30);  // resting on ground
                break;
            }
            s_pvy += 1; s_py += s_pvy; if (s_py > 0) { s_py = 0; s_pvy = 0; }
            if (s_g2) lv_obj_align(s_g2, LV_ALIGN_CENTER, -petx, ground - 30 + s_py);
            s_obst_x -= 6;                                        // 20fps -> ~120 px/s
            if (abs(s_obst_x - petx) < 26 && s_py > -32) {        // hit (not above the obstacle)
                s_obst_hit++; s_obst_x = (int)screen_width + 40;
                s_target = (uint8_t)(qt_random_u32() % 12); if (s_gbox_note) lv_label_set_text(s_gbox_note, NOTE_TXT[s_target]);
            } else if (s_obst_x < -20) {                          // cleared
                s_obst_passed++; s_obst_x = (int)screen_width + (qt_random_u32() % 50);
                s_target = (uint8_t)(qt_random_u32() % 12); if (s_gbox_note) lv_label_set_text(s_gbox_note, NOTE_TXT[s_target]);
            }
            if (s_g3) lv_obj_align(s_g3, LV_ALIGN_CENTER, s_obst_x - (int)screen_width / 2, ground - 15);
            if (s_gprog) { char p[16]; snprintf(p, sizeof p, "%d/%d", s_obst_passed, JAM_TARGET_JUMPS); lv_label_set_text(s_gprog, p); }
            if (s_obst_passed + s_obst_hit >= JAM_TARGET_JUMPS) { s_score = s_obst_passed * 100 / JAM_TARGET_JUMPS; game_finish(); }
        } break;
        case G_CLEAN: {
            // Clearing happens on tap/note (clean_spot); just time-out as a fallback.
            if (now >= s_game_deadline) { s_score = s_correct * 100 / 3; game_finish(); }
        } break;
        case G_HEAL: {   // Road Dash
            int W = (int)screen_width, H = (int)screen_height;
            int top = 46, bot = H - 26;
            int rows = CROSS_LANES + 2;          // start row + lanes + goal row
            int rowH = (bot - top) / rows;
            const int petw = 26, carw = 44, carh = 22;
            int petx = W / 2 - petw / 2;
            int pet_y = bot - rowH - petw / 2 - s_cr_step * rowH;   // step 0 = bottom
            if (s_g1) lv_obj_set_pos(s_g1, petx, pet_y);
            if (s_cr_invuln > 0) s_cr_invuln--;
            for (int i = 0; i < CROSS_LANES; i++) {
                s_car_x[i] += s_car_dir[i] * s_car_speed[i];
                if (s_car_x[i] > W) s_car_x[i] = -carw;
                if (s_car_x[i] < -carw) s_car_x[i] = W;
                int car_y = bot - rowH - carh / 2 - (i + 1) * rowH + rowH / 2 - carh / 2;
                if (s_car[i]) lv_obj_set_pos(s_car[i], s_car_x[i], car_y);
                if (s_cr_invuln == 0 && s_cr_step == i + 1 &&
                    s_car_x[i] < petx + petw && s_car_x[i] + carw > petx) {
                    if (s_cr_step > 0) s_cr_step--;     // bumped back a lane
                    s_cr_invuln = 16;                  // ~0.8s grace
                }
            }
            int shown = s_cr_step > CROSS_LANES ? CROSS_LANES : s_cr_step;
            if (s_gprog) { char p[8]; snprintf(p, sizeof p, "%d/%d", shown, CROSS_LANES); lv_label_set_text(s_gprog, p); }
            if (s_cr_step > CROSS_LANES) { s_score = 100; game_finish(); }
            else if (now >= s_game_deadline) { s_score = s_cr_step * 100 / (CROSS_LANES + 1); game_finish(); }
        } break;
        default: break;
    }
}

static void on_game_touch(lv_event_t *e) {
    (void)e;
    if (s_scene != SC_GAME || s_busy) return;
    if (s_game == G_JAM) { if (s_jam_running && s_py == 0) s_pvy = -13; }   // tap anywhere = jump
    else if (s_game == G_HEAL) { if (s_cr_step <= CROSS_LANES) s_cr_step++; }  // Road Dash: tap = hop
    // Clean specks are tapped directly (on_spot); Feed/Echo use their buttons.
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
static void jam_tick_cb(lv_timer_t *t) {
    (void)t;
    if (!s_root) return;
    uint32_t now = qt_uptime_ms();
    uint32_t age0 = s_pet.age_ticks;
    jama_tick(&s_pet, now);
    if (s_pet.age_ticks != age0) s_dirty = true;
    switch (s_scene) {
        case SC_PET:    update_pet(); pet_consume_pick(); break;
        case SC_MENU:   menu_consume_pick(); break;
        case SC_RESULT: result_consume_pick(); break;
        case SC_GAME:   game_update(); break;
        case SC_STATUS: break;
    }
    if (s_dirty && (now - s_last_commit_ms) >= COMMIT_MS) persist_now(true);
}

// ---------------------------------------------------------------------------
// display_frequency
// ---------------------------------------------------------------------------
static void jam_display_frequency(float frequency, float target_frequency,
                                  TunerNoteName note_name, int octave, float cents) {
    (void)frequency; (void)target_frequency;
    uint32_t now = qt_uptime_ms();
    s_onset = (s_prev_note == NOTE_NONE && note_name != NOTE_NONE);
    s_prev_note = note_name;
    if (note_name != NOTE_NONE) s_last_note_ms = now;

    if (s_menu_n > 0 && s_scene != SC_GAME) {
        if (note_name == NOTE_NONE) { s_saw_silence = true; return; }
        if (now < s_input_lock_until || !s_saw_silence) return;
        for (int i = 0; i < s_menu_n; i++)
            if (jama_note_matches((int)note_name, s_menu_note[i])) { s_menu_pick = i; s_saw_silence = false; break; }
        return;
    }
    if (s_scene == SC_GAME && now >= s_input_lock_until) game_on_pitch(note_name, octave, cents, s_onset);
}

// ---------------------------------------------------------------------------
// Interface
// ---------------------------------------------------------------------------
static const char *jam_get_name(void) { return "Jamagotchi"; }
static bool        jam_enable_screen(void) { return true; }

static void jam_init(lv_obj_t *screen) {
    s_screen = screen;
    s_dirty = false;
    s_last_commit_ms = qt_uptime_ms();
    s_next_blink = qt_uptime_ms() + 2000;
    s_call = JAMA_CALL_NONE;
    s_prev_note = NOTE_NONE; s_last_note_ms = 0;

    int32_t n = qt_state_get_blob(JAM_STATE_KEY, &s_pet, sizeof s_pet);
    if (n != (int32_t)sizeof s_pet || s_pet.schema_version != JAMA_SCHEMA_VERSION) { jama_init(&s_pet); s_dirty = true; }

    uint32_t now = qt_uptime_ms();
    if (s_left_uptime_ms != 0 && now > s_left_uptime_ms) { jama_action_practice_secs(&s_pet, (now - s_left_uptime_ms) / 1000u); s_dirty = true; }
    s_left_uptime_ms = 0;

    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, on_game_touch, LV_EVENT_PRESSED, NULL);

    s_root = lv_obj_create(screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_CLICKABLE);

    go_scene(SC_PET);
    build_pet();
    s_timer = lv_timer_create(jam_tick_cb, TICK_MS, NULL);
}

static void jam_cleanup(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_left_uptime_ms = qt_uptime_ms();
    qt_state_set_blob(JAM_STATE_KEY, &s_pet, sizeof s_pet);
    s_screen = NULL; s_root = NULL;
    s_body = s_eye_l = s_eye_r = s_mouth = s_speech = s_hint = NULL;
    s_gnote = s_gprog = s_ginstr = s_gbox_note = s_g1 = s_g2 = s_g3 = s_g4 = NULL;
    for (int i = 0; i < 4; i++) s_choice[i] = NULL;
    for (int i = 0; i < CROSS_LANES; i++) s_car[i] = NULL;
    for (int i = 0; i < ECHO_LEN; i++) { s_tile[i] = NULL; s_tile_lbl[i] = NULL; }
}
