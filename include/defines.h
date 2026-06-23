// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/**
 * @file defines.h  (PUBLIC SDK SUBSET)
 *
 * This is a TRIMMED version of the firmware's defines.h containing only the
 * constants relevant to plugin authors. All GPIO pin numbers, FreeRTOS task
 * configuration, ADC/relay/hardware internals, and smoothing filter internals
 * have been removed. The full version lives inside the closed-source firmware.
 *
 * Include this file indirectly via qtune_plugin.h — do not include it directly.
 */
#if !defined(TUNER_GLOBAL_DEFINES)
#define TUNER_GLOBAL_DEFINES

// tuner_math.h is written to the common C/C++ subset, so plugins of either
// language get TunerNoteName, name_for_note(), and compute_frequency_info().
#include "tuner_math.h"

// ---------------------------------------------------------------------------
// Reference pitch
// ---------------------------------------------------------------------------

/// Standard A4 reference frequency in Hz.
#define A4_FREQ 440.0

/// Cents per semitone (always 100 — useful for range-checking math).
#define CENTS_PER_SEMITONE 100

// ---------------------------------------------------------------------------
// Default user-facing settings
//
// These values reflect the firmware defaults. A plugin should always prefer
// to read live values at runtime via the qt_get_*() accessors in
// qtune_plugin_host_api.h. These constants are provided solely for use in
// compile-time defaults or documentation.
// ---------------------------------------------------------------------------

/// Default reference frequency (A4) used on first boot.
#define DEFAULT_REFERENCE_FREQUENCY A4_FREQ

/// Default half-width (in cents) of the "in-tune" window (±3 cents).
#define DEFAULT_IN_TUNE_CENTS_WIDTH ((uint8_t)3)

/// Default note-name color palette (LV_PALETTE_NONE = amber/retro scheme).
#define DEFAULT_NOTE_NAME_PALETTE ((lv_palette_t)LV_PALETTE_NONE)

/// Default: cents value is not shown.
#define DEFAULT_SHOW_CENTS (0)

/// Default: monitoring mode off.
#define DEFAULT_MONITORING_MODE (0)

// ---------------------------------------------------------------------------
// GUI constants available to plugins
// ---------------------------------------------------------------------------

/// Number of visual segments used for showing tuning accuracy (informational).
#define INDICATOR_SEGMENTS 100

/// UTF-8 gear symbol for reference (used in the firmware's settings button).
#define GEAR_SYMBOL "\xEF\x80\x93"

/// Duration (ms) of the last-note fade-out animation.
#define LAST_NOTE_FADE_INTERVAL_MS 2000

#endif // TUNER_GLOBAL_DEFINES
