// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// jama_quiz.h
//
// Pure-C kernels shared by the Jamagotchi mini-games (no LVGL, host-testable).
// Note classes are 0..11 in the firmware's TunerNoteName order (A=0, A#=1, B=2,
// C=3, ... G#=11). Octave is the scientific octave (increments at C).

#ifndef JAMA_QUIZ_H
#define JAMA_QUIZ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JAMA_NUM_NOTES 12

// An RNG callback (so tests are deterministic and the plugin passes qt_random_u32).
typedef uint32_t (*jama_rng_fn)(void);

// True if two note classes are the same pitch class (octave-agnostic).
bool jama_note_matches(int played, int target);

// True if |cents| is within the in-tune window.
bool jama_in_tune(float cents, int width_cents);

// Absolute, monotonically-increasing pitch index from a note class + octave
// (C-relative semitones: C=0..B=11, so higher pitch => higher index).
int jama_pitch_index(int note, int octave);

// Map a played pitch (note+octave) to a horizontal position in [x_min, x_max],
// clamping to the [lo_index, hi_index] pitch window. Used by Pitch Steer.
int jama_note_to_x(int note, int octave, int lo_index, int hi_index,
                   int x_min, int x_max);

// Compare a played sequence to a target (octave-agnostic); returns how many
// leading positions match. Caller treats == n as a win. Used by Echo.
int jama_sequence_match(const uint8_t *played, const uint8_t *target, int n);

// Fill out[0..n) with n DISTINCT note classes, none equal to `exclude` (pass a
// value >= JAMA_NUM_NOTES for "no exclusion"). Used for multiple-choice options
// and target sequences. Returns the count written (== n unless n is too large).
int jama_pick_distinct(uint8_t *out, int n, int exclude, jama_rng_fn rng);

#ifdef __cplusplus
}
#endif

#endif // JAMA_QUIZ_H
