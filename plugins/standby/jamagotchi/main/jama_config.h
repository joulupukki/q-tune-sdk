// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// jama_config.h
//
// All Jamagotchi balancing numbers in one place. Pure data + macros; no code, no
// LVGL, no firmware headers (the host unit tests include this too). The model is
// need-driven: hidden meters decay, cross thresholds, and raise "calls" the player
// answers with a mini-game; ignored calls become care mistakes that shape growth.

#ifndef JAMA_CONFIG_H
#define JAMA_CONFIG_H

#include <stdint.h>

// --- Tick cadence ----------------------------------------------------------
// One care-tick of decay per CARE_TICK_SEC of (monotonic) uptime. Age accrues
// only while powered, so calendar time to grow up is longer than the tick math.
#define JAMA_CARE_TICK_SEC      30u
#define JAMA_MAX_CATCHUP_TICKS  240u   // cap catch-up within one boot (~2h)

// --- Stat bounds & fresh-pet starting values -------------------------------
#define JAMA_STAT_MAX           100
#define JAMA_START_HUNGER       70
#define JAMA_START_HAPPINESS    70
#define JAMA_START_DISCIPLINE   50
#define JAMA_START_ENERGY       90
#define JAMA_START_HEALTH       100
#define JAMA_START_WEIGHT       30

// --- Per-care-tick decay ---------------------------------------------------
#define JAMA_DECAY_HUNGER       2
#define JAMA_DECAY_HAPPINESS    1
#define JAMA_DECAY_DISCIPLINE   1
#define JAMA_DECAY_ENERGY       1   // while awake
#define JAMA_ENERGY_RECOVER     5   // per tick while napping
#define JAMA_ENERGY_WAKE        100 // wakes when energy back to full

// --- Need thresholds (a meter at/under its threshold raises a call) ---------
#define JAMA_NEED_HUNGER        30
#define JAMA_NEED_HAPPINESS     30
#define JAMA_NEED_DISCIPLINE    25
#define JAMA_NEED_ENERGY        15   // at/under -> auto nap (not a call)

// --- Health / sickness ------------------------------------------------------
#define JAMA_HEALTH_DROP_NEGLECT 3   // per tick when hunger or happiness == 0
#define JAMA_HEALTH_DROP_POOP    2   // per tick while left dirty
#define JAMA_HEALTH_RECOVER      2   // per tick when fed, happy and clean
#define JAMA_SICK_HEALTH         20  // below -> sick
#define JAMA_WELL_HEALTH         40  // at/above -> not sick
#define JAMA_CARE_MISS_HEALTH    6   // health lost per ignored call

// Poop appears every N care-ticks (until cleaned).
#define JAMA_POOP_EVERY_TICKS    6

// --- Mini-game scoring (0..100 score from a game) --------------------------
// A score proportionally fills the matching need: gain = score% of FULL_GAIN.
#define JAMA_FULL_GAIN          100  // a perfect game fully tops up the need
#define JAMA_CLEAR_THRESHOLD    50   // score >= this clears boolean needs (poop/sick)
#define JAMA_FREEPLAY_GAIN_CAP  20   // free-play (Games menu) happiness gain cap/run

// --- Evolution / life stages -----------------------------------------------
// Musical life stages mapped onto the Tamagotchi life cycle. Baby can't die;
// teen care decides whether the adult is the good or the neglect variant.
typedef enum {
    JAMA_STAGE_BABY = 0,      // ~Baby: drains fast, cannot die
    JAMA_STAGE_CHILD,         // ~Child
    JAMA_STAGE_TEEN,          // ~Teen: care here gates the adult
    JAMA_STAGE_ADULT,         // ~Adult (well cared for)
    JAMA_STAGE_ADULT_NEGLECT, // ~Adult (neglected)
    JAMA_STAGE_COUNT,
} jama_stage_t;

// Age (in care-ticks) at which each life step happens. Tunable; only counts
// while the pedal is powered.
#define JAMA_AGE_CHILD          120u   // baby -> child  (~1h at 30s/tick)
#define JAMA_AGE_TEEN           600u   // child -> teen
#define JAMA_AGE_ADULT          1200u  // teen -> adult
// At the teen->adult step, care_score >= this -> good adult, else neglect adult.
#define JAMA_ADULT_CARE_MIN     55

// care_score = avg(happiness, discipline) - penalty per care miss, clamped 0..100.
#define JAMA_CARE_MISS_PENALTY   8

// --- Calls -----------------------------------------------------------------
// A raised call becomes a care mistake if ignored beyond this window of active
// uptime. (The UI tracks the timer and tells the core when it expires.)
#define JAMA_CALL_IGNORE_SEC    900u   // ~15 minutes

#endif // JAMA_CONFIG_H
