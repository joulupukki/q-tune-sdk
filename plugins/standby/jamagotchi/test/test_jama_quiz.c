// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// test_jama_quiz.c — host unit tests for the mini-game kernels.
//   cc -I../main test_jama_quiz.c ../main/jama_quiz.c -o /tmp/q && /tmp/q

#include "jama_quiz.h"

#include <stdio.h>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Deterministic RNG for repeatable tests.
static uint32_t g_seed = 12345u;
static uint32_t test_rng(void) { g_seed = g_seed * 1103515245u + 12345u; return g_seed >> 8; }

// Note classes (A=0): C=3, E=7, A=0.
enum { N_A = 0, N_C = 3, N_E = 7 };

static void test_note_matches(void) {
    printf("test_note_matches\n");
    CHECK(jama_note_matches(N_C, N_C));
    CHECK(jama_note_matches(N_C, N_C + 12));     // octave-agnostic
    CHECK(!jama_note_matches(N_C, N_E));
}

static void test_in_tune(void) {
    printf("test_in_tune\n");
    CHECK(jama_in_tune(2.0f, 3));
    CHECK(jama_in_tune(-3.0f, 3));
    CHECK(!jama_in_tune(5.0f, 3));
}

static void test_pitch_index_monotonic(void) {
    printf("test_pitch_index_monotonic\n");
    // E2 < A2 < C3 ... and B(oct4) < C(oct5) since octave changes at C.
    CHECK(jama_pitch_index(N_E, 2) < jama_pitch_index(N_A, 3));
    CHECK(jama_pitch_index(2 /*B*/, 4) < jama_pitch_index(N_C, 5));
    CHECK(jama_pitch_index(N_C, 4) < jama_pitch_index(N_C, 5));
}

static void test_note_to_x(void) {
    printf("test_note_to_x\n");
    int lo = jama_pitch_index(N_E, 2);   // low E
    int hi = jama_pitch_index(N_E, 5);   // high E
    CHECK(jama_note_to_x(N_E, 2, lo, hi, 0, 100) == 0);     // bottom -> x_min
    CHECK(jama_note_to_x(N_E, 5, lo, hi, 0, 100) == 100);   // top -> x_max
    int mid = jama_note_to_x(N_E, 1, lo, hi, 0, 100);       // below range -> clamp
    CHECK(mid == 0);
    int x = jama_note_to_x(N_E, 3, lo, hi, 0, 100);
    CHECK(x > 0 && x < 100);                                // somewhere in between
}

static void test_sequence_match(void) {
    printf("test_sequence_match\n");
    uint8_t target[4] = { N_C, N_E, N_A, 5 };
    uint8_t good[4]   = { N_C, N_E + 12, N_A, 5 };  // octave-agnostic full match
    uint8_t bad[4]    = { N_C, N_A, N_E, 5 };       // diverges at index 1
    CHECK(jama_sequence_match(good, target, 4) == 4);
    CHECK(jama_sequence_match(bad, target, 4) == 1);
}

static void test_pick_distinct(void) {
    printf("test_pick_distinct\n");
    uint8_t out[4];
    int n = jama_pick_distinct(out, 4, N_C, test_rng);
    CHECK(n == 4);
    for (int i = 0; i < n; i++) {
        CHECK(out[i] != N_C);                 // excluded value never appears
        CHECK(out[i] < JAMA_NUM_NOTES);
        for (int j = i + 1; j < n; j++) CHECK(out[i] != out[j]);  // distinct
    }
}

int main(void) {
    test_note_matches();
    test_in_tune();
    test_pitch_index_monotonic();
    test_note_to_x();
    test_sequence_match();
    test_pick_distinct();
    printf("\n%d/%d checks passed\n", g_total - g_fail, g_total);
    if (g_fail) { printf("FAILED (%d)\n", g_fail); return 1; }
    printf("OK\n");
    return 0;
}
