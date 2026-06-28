// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// jama_core.c — Jamagotchi need-driven game model (pure C, host-testable).

#include "jama_core.h"

#include <string.h>

// --- small clamps ----------------------------------------------------------
static int8_t cl(int v) {
    if (v < 0) return 0;
    if (v > JAMA_STAT_MAX) return (int8_t)JAMA_STAT_MAX;
    return (int8_t)v;
}
static uint8_t clu(int v) {
    if (v < 0) return 0;
    if (v > JAMA_STAT_MAX) return (uint8_t)JAMA_STAT_MAX;
    return (uint8_t)v;
}

// ---------------------------------------------------------------------------
void jama_init(jama_state_t *s) {
    memset(s, 0, sizeof(*s));
    s->schema_version = JAMA_SCHEMA_VERSION;
    s->stage      = JAMA_STAGE_BABY;
    s->hunger     = JAMA_START_HUNGER;
    s->happiness  = JAMA_START_HAPPINESS;
    s->discipline = JAMA_START_DISCIPLINE;
    s->energy     = JAMA_START_ENERGY;
    s->health     = JAMA_START_HEALTH;
    s->weight     = JAMA_START_WEIGHT;
    // last_tick_ms = 0 => jama_tick rebases on first call (no fast-forward).
}

bool jama_is_dead(const jama_state_t *s) {
    return s->health == 0;
}

uint8_t jama_care_score(const jama_state_t *s) {
    int base = ((int)s->happiness + (int)s->discipline) / 2;
    int v = base - (int)s->care_misses * JAMA_CARE_MISS_PENALTY;
    return clu(v);
}

const char *jama_stage_name(uint8_t stage) {
    switch (stage) {
        case JAMA_STAGE_BABY:          return "Rookie";
        case JAMA_STAGE_CHILD:         return "Busker";
        case JAMA_STAGE_TEEN:          return "Open Mic";
        case JAMA_STAGE_ADULT:         return "Headliner";
        case JAMA_STAGE_ADULT_NEGLECT: return "Lounge Act";
        default:                       return "?";
    }
}

const char *jama_call_word(jama_call_t call) {
    switch (call) {
        case JAMA_CALL_SICK:       return "sick";
        case JAMA_CALL_DIRTY:      return "messy";
        case JAMA_CALL_HUNGRY:     return "hungry";
        case JAMA_CALL_BORED:      return "bored";
        case JAMA_CALL_DISCIPLINE: return "restless";
        default:                   return "ok";
    }
}

static bool is_baby(const jama_state_t *s) { return s->stage == JAMA_STAGE_BABY; }

static void try_evolve(jama_state_t *s) {
    if (s->stage == JAMA_STAGE_BABY && s->age_ticks >= JAMA_AGE_CHILD) {
        s->stage = JAMA_STAGE_CHILD;
    } else if (s->stage == JAMA_STAGE_CHILD && s->age_ticks >= JAMA_AGE_TEEN) {
        s->stage = JAMA_STAGE_TEEN;
    } else if (s->stage == JAMA_STAGE_TEEN && s->age_ticks >= JAMA_AGE_ADULT) {
        // Teen care decides the adult form.
        s->stage = (jama_care_score(s) >= JAMA_ADULT_CARE_MIN)
                       ? JAMA_STAGE_ADULT : JAMA_STAGE_ADULT_NEGLECT;
    }
}

// One care-tick of the simulation.
static void care_tick(jama_state_t *s) {
    s->age_ticks++;
    const int m = is_baby(s) ? 2 : 1;   // babies drain faster

    if (s->sleeping) {
        s->energy = cl((int)s->energy + JAMA_ENERGY_RECOVER);
        s->hunger = cl((int)s->hunger - JAMA_DECAY_HUNGER * m / 2);
        if (s->energy >= JAMA_ENERGY_WAKE) s->sleeping = false;
    } else {
        s->hunger     = cl((int)s->hunger     - JAMA_DECAY_HUNGER * m);
        s->happiness  = cl((int)s->happiness  - JAMA_DECAY_HAPPINESS * m);
        s->discipline = cl((int)s->discipline - JAMA_DECAY_DISCIPLINE);
        s->energy     = cl((int)s->energy     - JAMA_DECAY_ENERGY);
        if (s->energy <= JAMA_NEED_ENERGY) s->sleeping = true;  // tired -> nap
    }

    // Poop cadence (babies poop more via faster ticks already).
    s->ticks_since_poop++;
    if (!s->pooped && s->ticks_since_poop >= JAMA_POOP_EVERY_TICKS) {
        s->pooped = true;
        s->ticks_since_poop = 0;
    }

    // Health.
    bool neglected = (s->hunger <= 0) || (s->happiness <= 0);
    int dh = 0;
    if (neglected) dh -= JAMA_HEALTH_DROP_NEGLECT;
    if (s->pooped) dh -= JAMA_HEALTH_DROP_POOP;
    if (!neglected && !s->pooped) dh += JAMA_HEALTH_RECOVER;
    s->health = clu((int)s->health + dh);
    if (is_baby(s) && s->health == 0) s->health = 1;  // baby cannot die

    if (neglected && s->care_misses < 255) s->care_misses++;

    if (s->health < JAMA_SICK_HEALTH) s->sick = true;
    else if (s->health >= JAMA_WELL_HEALTH) s->sick = false;

    try_evolve(s);
}

void jama_tick(jama_state_t *s, uint32_t now_ms) {
    if (jama_is_dead(s)) return;
    if (s->last_tick_ms == 0 || now_ms < s->last_tick_ms) {
        s->last_tick_ms = now_ms;   // fresh pet or reboot: rebase, don't fast-forward
        return;
    }
    const uint32_t period = JAMA_CARE_TICK_SEC * 1000u;
    if (period == 0) return;
    uint32_t n = (now_ms - s->last_tick_ms) / period;
    if (n == 0) return;
    if (n >= JAMA_MAX_CATCHUP_TICKS) {
        n = JAMA_MAX_CATCHUP_TICKS;
        s->last_tick_ms = now_ms;
    } else {
        s->last_tick_ms += n * period;
    }
    for (uint32_t i = 0; i < n && !jama_is_dead(s); i++) care_tick(s);
}

// ---------------------------------------------------------------------------
jama_call_t jama_pending_call(const jama_state_t *s) {
    if (jama_is_dead(s) || s->sleeping) return JAMA_CALL_NONE;
    if (s->sick)                              return JAMA_CALL_SICK;
    if (s->pooped)                            return JAMA_CALL_DIRTY;
    if (s->hunger     <= JAMA_NEED_HUNGER)    return JAMA_CALL_HUNGRY;
    if (s->happiness  <= JAMA_NEED_HAPPINESS) return JAMA_CALL_BORED;
    if (s->discipline <= JAMA_NEED_DISCIPLINE) return JAMA_CALL_DISCIPLINE;
    return JAMA_CALL_NONE;
}

void jama_satisfy_call(jama_state_t *s, jama_call_t call, uint8_t score) {
    if (jama_is_dead(s)) return;
    if (score > 100) score = 100;
    int gain = (int)score * JAMA_FULL_GAIN / 100;
    bool clears = score >= JAMA_CLEAR_THRESHOLD;
    switch (call) {
        case JAMA_CALL_HUNGRY:
            s->hunger = cl((int)s->hunger + gain);
            s->weight = clu((int)s->weight + 1);
            break;
        case JAMA_CALL_BORED:
            s->happiness = cl((int)s->happiness + gain);
            s->weight    = clu((int)s->weight - 1);
            s->jam_wins++;
            break;
        case JAMA_CALL_DISCIPLINE:
            s->discipline = cl((int)s->discipline + gain);
            break;
        case JAMA_CALL_DIRTY:
            if (clears) s->pooped = false;
            s->health = clu((int)s->health + gain / 2);
            break;
        case JAMA_CALL_SICK:
            s->health = clu((int)s->health + gain);
            if (clears || s->health >= JAMA_WELL_HEALTH) s->sick = false;
            break;
        default:
            break;
    }
}

void jama_register_care_miss(jama_state_t *s) {
    if (jama_is_dead(s)) return;
    if (s->care_misses < 255) s->care_misses++;
    int h = (int)s->health - JAMA_CARE_MISS_HEALTH;
    s->health = clu(h);
    if (is_baby(s) && s->health == 0) s->health = 1;
}

void jama_action_practice_secs(jama_state_t *s, uint32_t secs) {
    if (jama_is_dead(s) || secs == 0) return;
    if (secs > 3600u) secs = 3600u;
    s->practice_secs += secs;
    int gain = (int)(secs / 20u);   // ~20 s of tuning => +1 hunger
    if (gain > 0) s->hunger = cl((int)s->hunger + gain);
}

void jama_freeplay_reward(jama_state_t *s, uint8_t score) {
    if (jama_is_dead(s)) return;
    if (score > 100) score = 100;
    int gain = (int)score * JAMA_FREEPLAY_GAIN_CAP / 100;  // capped
    s->happiness = cl((int)s->happiness + gain);
}
