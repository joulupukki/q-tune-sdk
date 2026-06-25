// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

#if !defined(TUNER_UI_INTERFACE)
#define TUNER_UI_INTERFACE

#include <stdlib.h>

#include "lvgl.h"
#include "defines.h"

typedef enum {
    /// A typical tuner available normally from the list of tuners.
    TuningUITypeStandard,

    /// These types of tuners only appear in the tuning list if utility tuners
    /// are enabled in the advanced settings menu options.
    TuningUITypeUtility
} TuningUIType;

/// @brief Implement a tuner UI by implementing this interface.
///
/// NOTE: a plugin does NOT carry its own numeric ID. The firmware assigns a
/// number dynamically at load time; your plugin's stable identity is the `uid`
/// string in its QTunePluginDescriptor (the SDK scaffolding tool generates one).
typedef struct {
    /// @brief Returns the name of the tuning interface that will be shown in
    /// user settings.
    const char * (*get_name)(void);

    TuningUIType (*get_type)(void);

    /// @brief Initialize the tuner UI.
    ///
    /// Interfaces should save a local copy of the screen since it will not be
    /// passed in subsequent calls. The `init()` function will be called at boot
    /// for the active Tuner GUI. If the user enters exits tuning mode,
    /// `cleanup()` will be called and `init()` will be called when tuning mode
    /// is reactivated.
    ///
    /// You should prefer to create items in memory that are needed to support
    /// the tuner in this init function and clean them up in the `cleanup()`
    /// function. This will allow the user to switch to other tuning UIs without
    /// causing problems with memory allocation.
    void (*init)(lv_obj_t *screen);

    /// @brief Display the frequency/note/cents/etc.
    /// @param frequency The detected frequency in Hz.
    /// @param target_frequency The target frequency in Hz.
    /// @param note_name The note name (e.g. A, B, C, etc.).
    /// @param octave The octave number of the detected note.
    /// @param cents The number of cents off from the note (e.g. -50, 0, 50).
    /// @param show_mute_indicator True if the tuner is in tuning mode and
    /// should show the mute indicator (because of monitoring mode).
    void (*display_frequency)(float frequency, float target_frequency, TunerNoteName note_name, int octave, float cents, bool show_mute_indicator);

    /// @brief Set the alignment on the settings button.
    void (*align_settings_button)(lv_obj_t *btn);

    /// @brief Perform any cleanup needed (this UI is being deactivated).
    ///
    /// The `cleanup()` function is called when the user enters tuning mode.
    /// The `init()` function is then called when the user exits tuning mode.
    /// If a user chooses a different Tuner GUI in user settings, only that
    /// Tuner GUI will be initialized (and shown).
    ///
    /// The tuner_gui_task takes care of cleaning up the main screen so
    /// Tuner GUI interfaces are not responsible for cleaning up the main
    /// screen. The tuner_gui_task will also remove any LVGL objects that were
    /// placed in the screen by the Tuner GUI interface.
    ///
    /// If you have any animations or timers running make sure to stop them
    /// here.
    void (*cleanup)(void);
} TunerGUIInterface;

#endif
