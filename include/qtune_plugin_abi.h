// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// qtune_plugin_abi.h
//
// Stable ABI contract shared between the Q-Tune firmware and externally-compiled
// UI plugins. This header is shipped verbatim in the public plugin SDK, so keep
// it dependency-light: it must compile in both the firmware and a standalone
// plugin build. The firmware stays a closed binary; plugins only ever see this
// contract plus the curated symbol export table.
//
// COMPATIBILITY RULES
//   - The plugin ABI version is split into MAJOR.MINOR (see the macros below):
//       * Bump MAJOR (and reset MINOR to 0) on a BREAKING change — anything that
//         alters the binary layout of QTunePluginDescriptor, TunerGUIInterface,
//         TunerStandbyGUIInterface, or the meaning of existing exported symbols.
//         The loader rejects any plugin whose MAJOR differs.
//       * Bump MINOR on a BACKWARD-COMPATIBLE additive change — e.g. a new callback
//         appended to the END of an interface struct, or a new host accessor. The
//         loader accepts a plugin whose MINOR is <= the firmware's, so plugins
//         built against an older minor keep working; a plugin that needs a newer
//         minor is rejected on firmware with an older minor.
//     There is deliberately no PATCH: a change that does not alter the binary
//     contract isn't an ABI change — track it with the firmware version instead.
//   - LVGL struct layouts are baked into a plugin at compile time, so a plugin and
//     the firmware MUST share the same LVGL major.minor. The loader rejects on a
//     major.minor mismatch (patch drift is allowed).

#ifndef QTUNE_PLUGIN_ABI_H
#define QTUNE_PLUGIN_ABI_H

#include <stdint.h>
#include "lvgl.h" // brings in LVGL_VERSION_* via lv_version.h

#ifdef __cplusplus
extern "C" {
#endif

// Plugin ABI version, split into major.minor (see COMPATIBILITY RULES above).
// MAJOR bumps on a breaking change; MINOR bumps on a backward-compatible addition.
#define QTUNE_PLUGIN_ABI_MAJOR 1u
#define QTUNE_PLUGIN_ABI_MINOR 0u

// Packed major.minor: major in the top 16 bits, minor in the low 16.
#define QTUNE_PLUGIN_ABI_VERSION \
    (((uint32_t)QTUNE_PLUGIN_ABI_MAJOR << 16) | (uint32_t)QTUNE_PLUGIN_ABI_MINOR)

// Extract the major / minor halves of a packed ABI version.
#define QTUNE_PLUGIN_ABI_GET_MAJOR(v) ((uint32_t)(v) >> 16)
#define QTUNE_PLUGIN_ABI_GET_MINOR(v) ((uint32_t)(v) & 0xFFFFu)

// Packed LVGL version the firmware/plugin was built against: 0xMMmmpp.
#define QTUNE_LVGL_VERSION \
    (((uint32_t)LVGL_VERSION_MAJOR << 16) | \
     ((uint32_t)LVGL_VERSION_MINOR << 8)  | \
     ((uint32_t)LVGL_VERSION_PATCH))

// Loader compares only the major.minor portion (top 16 bits) for compatibility.
#define QTUNE_LVGL_VERSION_COMPAT(v) ((uint32_t)(v) >> 8)

typedef enum {
    QTUNE_PLUGIN_TUNER   = 1, // interface points to a TunerGUIInterface
    QTUNE_PLUGIN_STANDBY = 2, // interface points to a TunerStandbyGUIInterface
} QTunePluginType;

// Every plugin exports exactly one instance of this struct with C linkage under
// the symbol name QTUNE_PLUGIN_DESCRIPTOR_SYMBOL. The loader dlsym()s it, validates
// the version fields, then casts `interface` according to `type`.
typedef struct {
    uint32_t        abi_version;  // packed major.minor; loader needs same major, minor <= firmware
    uint32_t        lvgl_version; // QTUNE_LVGL_VERSION at plugin build time
    QTunePluginType type;
    const char     *sdk_build;    // freeform SDK/build tag, for logging only
    const void     *interface;    // -> TunerGUIInterface* or TunerStandbyGUIInterface*
    const char     *uid;          // STABLE plugin identity (see below). Required.
} QTunePluginDescriptor;

// `uid` is the plugin's permanent identity — the firmware persists the user's UI
// selection and tracks crashes by this string, NOT by a number. It must be:
//   * STABLE: never change it once a plugin is published (changing it loses the
//     user's saved selection of that UI).
//   * UNIQUE: namespaced so two unrelated plugins never collide WITHOUT any central
//     registry. The SDK scaffolding tool generates one automatically, e.g.
//     "qtune.my-tuner.k7f2q9". It must NOT be a bare integer — that space is
//     reserved for built-in UIs, whose uid is simply their fixed number as a string.
// The firmware assigns each plugin a numeric slot DYNAMICALLY at load time (within
// the reserved ranges below); plugins no longer choose a number themselves.

// Well-known data symbol holding the descriptor. The offline SDK validator
// reads this statically from the .so to check the version fields. NOTE: the
// firmware does NOT use this symbol at load time — see the entry function below.
#define QTUNE_PLUGIN_DESCRIPTOR_SYMBOL "qtune_plugin_descriptor"

// The ESP32 ELF loader resolves only FUNCTION (STT_FUNC) symbols in a loaded
// module via dlsym() — never data objects. So every plugin MUST also export an
// entry FUNCTION under this name that returns a pointer to its descriptor; the
// firmware calls it to obtain the descriptor at load time:
//
//   extern "C" QTUNE_PLUGIN_EXPORT
//   const QTunePluginDescriptor *qtune_plugin_entry(void) {
//       return &qtune_plugin_descriptor;
//   }
#define QTUNE_PLUGIN_ENTRY_SYMBOL "qtune_plugin_entry"
typedef const QTunePluginDescriptor *(*QTunePluginEntryFn)(void);

// The plugin SDK builds .so files with -fvisibility=hidden and --strip-all, so
// exported symbols must be explicitly given default visibility to survive into
// .dynsym. Declare the descriptor and the entry function with this macro.
#define QTUNE_PLUGIN_EXPORT __attribute__((used, visibility("default")))

// Reserved ID ranges for plugin-provided UIs so they never collide with built-ins.
// (Built-in tuners use 0..~9 + 255; built-in standby uses 0,1 + 200,201.)
#define QTUNE_PLUGIN_TUNER_ID_MIN    100u
#define QTUNE_PLUGIN_TUNER_ID_MAX    199u
#define QTUNE_PLUGIN_STANDBY_ID_MIN  210u
#define QTUNE_PLUGIN_STANDBY_ID_MAX  254u

#ifdef __cplusplus
} // extern "C"
#endif

#endif // QTUNE_PLUGIN_ABI_H
