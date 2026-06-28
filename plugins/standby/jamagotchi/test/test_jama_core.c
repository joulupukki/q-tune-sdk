// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// test_jama_core.c — host unit tests for the need-driven game model.
//   cc -I../main test_jama_core.c ../main/jama_core.c -o /tmp/t && /tmp/t
// (or run test/run.sh). Exit 0 = all passed.

#include "jama_core.h"

#include <stdio.h>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define PERIOD_MS (JAMA_CARE_TICK_SEC * 1000u)

static void advance(jama_state_t *s, uint32_t *t, int ticks) {
    *t += (uint32_t)ticks * PERIOD_MS;
    jama_tick(s, *t);
}

static void test_fresh_defaults(void) {
    printf("test_fresh_defaults\n");
    jama_state_t s; jama_init(&s);
    CHECK(s.schema_version == JAMA_SCHEMA_VERSION);
    CHECK(s.stage == JAMA_STAGE_BABY);
    CHECK(s.health == JAMA_START_HEALTH);
    CHECK(!jama_is_dead(&s));
    CHECK(jama_pending_call(&s) == JAMA_CALL_NONE);
}

static void test_decay_and_call(void) {
    printf("test_decay_and_call\n");
    jama_state_t s; jama_init(&s);
    uint32_t t = 1000; jama_tick(&s, t);   // rebase
    int8_t h0 = s.hunger;
    advance(&s, &t, 3);
    CHECK(s.hunger < h0);
    // Drive hunger under the threshold -> HUNGRY call.
    s.hunger = JAMA_NEED_HUNGER - 1;
    CHECK(jama_pending_call(&s) == JAMA_CALL_HUNGRY);
}

static void test_call_priority(void) {
    printf("test_call_priority\n");
    jama_state_t s; jama_init(&s);
    s.hunger = 0; s.happiness = 0;     // both needs unmet
    s.pooped = true; s.sick = true;
    CHECK(jama_pending_call(&s) == JAMA_CALL_SICK);   // sick wins
    s.sick = false;
    CHECK(jama_pending_call(&s) == JAMA_CALL_DIRTY);  // then dirty
    s.pooped = false;
    CHECK(jama_pending_call(&s) == JAMA_CALL_HUNGRY); // then hungry
}

static void test_satisfy_proportional(void) {
    printf("test_satisfy_proportional\n");
    jama_state_t s; jama_init(&s);
    s.hunger = 0;
    jama_satisfy_call(&s, JAMA_CALL_HUNGRY, 70);  // 70% -> +70 hunger
    CHECK(s.hunger == 70);
    // A high score clears a boolean need; a low score does not.
    s.pooped = true;
    jama_satisfy_call(&s, JAMA_CALL_DIRTY, 30);
    CHECK(s.pooped);
    jama_satisfy_call(&s, JAMA_CALL_DIRTY, 80);
    CHECK(!s.pooped);
}

static void test_care_miss(void) {
    printf("test_care_miss\n");
    jama_state_t s; jama_init(&s);
    s.stage = JAMA_STAGE_CHILD;       // past baby so health can move freely
    uint8_t h0 = s.health, m0 = s.care_misses;
    jama_register_care_miss(&s);
    CHECK(s.care_misses == m0 + 1);
    CHECK(s.health == h0 - JAMA_CARE_MISS_HEALTH);
}

static void test_baby_cannot_die(void) {
    printf("test_baby_cannot_die\n");
    jama_state_t s; jama_init(&s);
    uint32_t t = 1000; jama_tick(&s, t);
    s.hunger = 0; s.happiness = 0;
    advance(&s, &t, 100);             // still a baby (< JAMA_AGE_CHILD)
    CHECK(s.stage == JAMA_STAGE_BABY);
    CHECK(!jama_is_dead(&s));         // baby cannot die
    CHECK(s.health >= 1);
}

static void test_child_can_die(void) {
    printf("test_child_can_die\n");
    jama_state_t s; jama_init(&s);
    uint32_t t = 1000; jama_tick(&s, t);
    s.hunger = 0; s.happiness = 0;
    // Age past the baby stage, then sustained neglect kills it.
    advance(&s, &t, (int)JAMA_AGE_CHILD + 200);
    CHECK(s.stage >= JAMA_STAGE_CHILD);
    CHECK(jama_is_dead(&s));
    uint32_t age = s.age_ticks;       // a dead pet stops aging
    advance(&s, &t, 10);
    CHECK(s.age_ticks == age);
}

static void test_evolution_gating(void) {
    printf("test_evolution_gating\n");
    // Good care at the teen->adult step yields the good adult.
    jama_state_t g; jama_init(&g);
    g.stage = JAMA_STAGE_TEEN; g.age_ticks = JAMA_AGE_ADULT - 1;
    g.happiness = 90; g.discipline = 90; g.care_misses = 0;
    g.last_tick_ms = 0;
    uint32_t t = 100000; jama_tick(&g, t); jama_tick(&g, t + PERIOD_MS);
    CHECK(g.stage == JAMA_STAGE_ADULT);

    // Poor care yields the neglect adult.
    jama_state_t b; jama_init(&b);
    b.stage = JAMA_STAGE_TEEN; b.age_ticks = JAMA_AGE_ADULT - 1;
    b.happiness = 10; b.discipline = 10; b.care_misses = 5;
    b.last_tick_ms = 0;
    uint32_t t2 = 100000; jama_tick(&b, t2); jama_tick(&b, t2 + PERIOD_MS);
    CHECK(b.stage == JAMA_STAGE_ADULT_NEGLECT);
}

static void test_freeplay_capped(void) {
    printf("test_freeplay_capped\n");
    jama_state_t s; jama_init(&s);
    s.happiness = 0;
    jama_freeplay_reward(&s, 100);    // capped, not a full 100
    CHECK(s.happiness == JAMA_FREEPLAY_GAIN_CAP);
}

static void test_reboot_guard(void) {
    printf("test_reboot_guard\n");
    jama_state_t s; jama_init(&s);
    uint32_t t = 100000; jama_tick(&s, t);
    advance(&s, &t, 5);
    uint32_t age = s.age_ticks;
    jama_tick(&s, 500);               // uptime went backwards (reboot)
    CHECK(s.age_ticks == age);
    jama_tick(&s, 500 + 3 * PERIOD_MS);
    CHECK(s.age_ticks == age + 3);
}

int main(void) {
    test_fresh_defaults();
    test_decay_and_call();
    test_call_priority();
    test_satisfy_proportional();
    test_care_miss();
    test_baby_cannot_die();
    test_child_can_die();
    test_evolution_gating();
    test_freeplay_capped();
    test_reboot_guard();
    printf("\n%d/%d checks passed\n", g_total - g_fail, g_total);
    if (g_fail) { printf("FAILED (%d)\n", g_fail); return 1; }
    printf("OK\n");
    return 0;
}
