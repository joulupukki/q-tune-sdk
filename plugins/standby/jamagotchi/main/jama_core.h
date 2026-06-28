// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// jama_core.h
//
// The Jamagotchi virtual-pet game model: a pure-C, need-driven state machine with
// NO LVGL and NO firmware dependencies, so it compiles and unit-tests on a desktop.
// Hidden meters decay over time; when one crosses its threshold the pet raises a
// "call". The UI answers a call with a mini-game and reports a 0..100 score, which
// proportionally fills the need; ignored calls are registered as care mistakes that
// shape evolution. All balancing lives in jama_config.h.

#ifndef JAMA_CORE_H
#define JAMA_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "jama_config.h"   // jama_stage_t, tuning constants

#ifdef __cplusplus
extern "C" {
#endif

#define JAMA_SCHEMA_VERSION 3   // bump when the struct layout changes

// Care needs the pet can call for. Priority order is encoded in jama_pending_call.
typedef enum {
    JAMA_CALL_NONE = 0,
    JAMA_CALL_SICK,        // needs healing
    JAMA_CALL_DIRTY,       // needs cleaning (pooped)
    JAMA_CALL_HUNGRY,      // needs feeding
    JAMA_CALL_BORED,       // needs play
    JAMA_CALL_DISCIPLINE,  // acting up; needs settling
} jama_call_t;

// The full pet state — persisted to NVS (qt_state_*), so keep it POD + versioned.
typedef struct {
    uint8_t  schema_version;
    uint8_t  stage;            // jama_stage_t
    int8_t   hunger;           // 0..100
    int8_t   happiness;        // 0..100
    int8_t   discipline;       // 0..100
    int8_t   energy;           // 0..100
    uint8_t  health;           // 0..100
    uint8_t  weight;
    bool     sleeping;
    bool     pooped;
    bool     sick;
    uint8_t  care_misses;
    uint32_t age_ticks;
    uint32_t ticks_since_poop;
    uint32_t practice_secs;    // lifetime tuner-use credit
    uint32_t jam_wins;
    uint32_t last_tick_ms;     // uptime of last applied care-tick
} jama_state_t;

// Initialise a brand-new pet (a fresh "egg"). Also used to restart after death.
void jama_init(jama_state_t *s);

// Advance the simulation to monotonic uptime now_ms, applying whole care-ticks
// elapsed since the last call (capped; reboot-safe; no-op for a dead pet).
void jama_tick(jama_state_t *s, uint32_t now_ms);

// The pet's current most-urgent call (or JAMA_CALL_NONE if content / napping).
jama_call_t jama_pending_call(const jama_state_t *s);

// Answer a call with a 0..100 score; proportionally fills the matching need.
// Boolean needs (DIRTY/SICK) clear when score >= JAMA_CLEAR_THRESHOLD.
void jama_satisfy_call(jama_state_t *s, jama_call_t call, uint8_t score);

// Record that a raised call was ignored too long (a care mistake).
void jama_register_care_miss(jama_state_t *s);

// Tuner-use credit: seconds spent tuning convert to hunger ("practice").
void jama_action_practice_secs(jama_state_t *s, uint32_t secs);

// Free-play (Games menu) happiness reward, capped so it can't be farmed.
void jama_freeplay_reward(jama_state_t *s, uint8_t score);

// --- Queries ---------------------------------------------------------------
bool        jama_is_dead(const jama_state_t *s);
uint8_t     jama_care_score(const jama_state_t *s);   // 0..100
const char *jama_stage_name(uint8_t stage);
const char *jama_call_word(jama_call_t call);         // "hungry", "bored", ...

#ifdef __cplusplus
}
#endif

#endif // JAMA_CORE_H
