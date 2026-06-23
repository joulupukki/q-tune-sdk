// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/*
 * Copyright (c) 2026 Boyd Timothy. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file tuner_math.h
 * @brief Pure pitch math — no ESP32, FreeRTOS, or LVGL dependencies.
 *
 * This header is safe to include in host-native unit tests. The firmware
 * includes it via defines.h and calls compute_frequency_info() from within
 * get_frequency_info() in pitch_detector_task.cpp.
 *
 * In the plugin SDK this header is compiled by BOTH C and C++ plugins, so it is
 * written to the common C/C++ subset (C headers, NULL, isfinite(), static
 * inline). Plugins built with elf_loader's project_so() are compiled as C.
 */
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>  // NULL
#include <stdbool.h> // bool/true/false in C

typedef enum {
    NOTE_A = 0,
    NOTE_A_SHARP,
    NOTE_B,
    NOTE_C,
    NOTE_C_SHARP,
    NOTE_D,
    NOTE_D_SHARP,
    NOTE_E,
    NOTE_F,
    NOTE_F_SHARP,
    NOTE_G,
    NOTE_G_SHARP,
    NOTE_NONE // also doubles as the total number of named notes
} TunerNoteName;

static inline const char *name_for_note(TunerNoteName note) {
    switch (note) {
        case NOTE_C:       return "C";
        case NOTE_C_SHARP: return "C#";
        case NOTE_D:       return "D";
        case NOTE_D_SHARP: return "D#";
        case NOTE_E:       return "E";
        case NOTE_F:       return "F";
        case NOTE_F_SHARP: return "F#";
        case NOTE_G:       return "G";
        case NOTE_G_SHARP: return "G#";
        case NOTE_A:       return "A";
        case NOTE_A_SHARP: return "A#";
        case NOTE_B:       return "B";
        case NOTE_NONE:    return "-";
        default:           return "?";
    }
}

typedef struct {
    float frequency;
    float cents;
    float targetFrequency;
    TunerNoteName targetNote;
    int targetOctave;
} FrequencyInfo;

// Plain enum (no fixed underlying type) so this compiles as C as well as C++.
typedef enum {
    tunerBypassTypeTrue = 0,
    tunerBypassTypeBuffered,
} TunerBypassType;

/**
 * @brief Compute the closest chromatic note and cent deviation for a frequency.
 *
 * Octave numbering follows scientific pitch convention (the octave changes at C,
 * not at A), so B3 (246.94 Hz) is correctly reported as octave 3.
 *
 * @param input_freq     Input frequency in Hz — must be > 0.
 * @param reference_freq Reference pitch for A4 in Hz (typically 440.0) — must be > 0.
 * @param freqInfo       Output struct; populated on success.
 * @return true on success; false if either frequency argument is <= 0.
 */
static inline bool compute_frequency_info(float input_freq, float reference_freq, FrequencyInfo *freqInfo) {
    if (freqInfo == NULL) {
        return false;
    }

    if (!isfinite(input_freq) || !isfinite(reference_freq)) {
        return false;
    }

    if (input_freq <= 0.0f || reference_freq <= 0.0f) {
        return false;
    }

    // Semitone distance from reference A4
    float semitone_offset = 12.0f * log2f(input_freq / reference_freq);
    int closest_semitone  = (int)roundf(semitone_offset);

    // Note index within the chromatic scale (0 = A, 1 = A#, …, 11 = G#)
    int note_index = closest_semitone % 12;
    if (note_index < 0) {
        note_index += 12;
    }

    // Scientific pitch octave: shift +9 semitones so the reference moves from
    // A4 to C4, then floor-divide by 12. C++ integer division truncates toward
    // zero, so we subtract 1 when the remainder is negative (floor semantics).
    int adj    = closest_semitone + 9;
    int octave = 4 + adj / 12 - (adj % 12 < 0 ? 1 : 0);

    float closest_note_freq = reference_freq * powf(2.0f, (float)closest_semitone / 12.0f);

    freqInfo->frequency       = input_freq;
    freqInfo->targetFrequency = closest_note_freq;
    freqInfo->cents           = 1200.0f * log2f(input_freq / closest_note_freq);
    freqInfo->targetNote      = (TunerNoteName)note_index;
    freqInfo->targetOctave    = octave;
    return true;
}
