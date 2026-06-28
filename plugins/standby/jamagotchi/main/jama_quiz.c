// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// jama_quiz.c — pure-C mini-game kernels. See jama_quiz.h.

#include "jama_quiz.h"

#include <math.h>

// TunerNoteName order is A=0, A#=1, B=2, C=3, C#=4, D=5, D#=6, E=7, F=8, F#=9,
// G=10, G#=11. Convert to C-relative semitone (C=0..B=11) so a larger value means
// a higher pitch within an octave.
static int semitone_from_c(int note_a_index) {
    // index:  A  A# B  C  C# D  D# E  F  F# G  G#
    // fromC:  9  10 11 0  1  2  3  4  5  6  7  8
    static const int LUT[JAMA_NUM_NOTES] = { 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    int i = note_a_index % JAMA_NUM_NOTES;
    if (i < 0) i += JAMA_NUM_NOTES;
    return LUT[i];
}

bool jama_note_matches(int played, int target) {
    int a = played % JAMA_NUM_NOTES; if (a < 0) a += JAMA_NUM_NOTES;
    int b = target % JAMA_NUM_NOTES; if (b < 0) b += JAMA_NUM_NOTES;
    return a == b;
}

bool jama_in_tune(float cents, int width_cents) {
    return fabsf(cents) <= (float)width_cents;
}

int jama_pitch_index(int note, int octave) {
    return octave * JAMA_NUM_NOTES + semitone_from_c(note);
}

int jama_note_to_x(int note, int octave, int lo_index, int hi_index,
                   int x_min, int x_max) {
    if (hi_index <= lo_index) return x_min;
    int idx = jama_pitch_index(note, octave);
    if (idx < lo_index) idx = lo_index;
    if (idx > hi_index) idx = hi_index;
    long span = (long)(hi_index - lo_index);
    long pos  = (long)(idx - lo_index);
    return x_min + (int)((long)(x_max - x_min) * pos / span);
}

int jama_sequence_match(const uint8_t *played, const uint8_t *target, int n) {
    int matched = 0;
    for (int i = 0; i < n; i++) {
        if (!jama_note_matches(played[i], target[i])) break;
        matched++;
    }
    return matched;
}

int jama_pick_distinct(uint8_t *out, int n, int exclude, jama_rng_fn rng) {
    if (n > JAMA_NUM_NOTES) n = JAMA_NUM_NOTES;
    int count = 0;
    int guard = 0;
    while (count < n && guard < 1000) {
        guard++;
        uint8_t cand = (uint8_t)(rng() % JAMA_NUM_NOTES);
        if ((int)cand == exclude) continue;
        bool dup = false;
        for (int i = 0; i < count; i++) {
            if (out[i] == cand) { dup = true; break; }
        }
        if (!dup) out[count++] = cand;
    }
    return count;
}
