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
//   - Bump QTUNE_PLUGIN_ABI_VERSION whenever the layout of QTunePluginDescriptor,
//     TunerGUIInterface, TunerStandbyGUIInterface, or the exported-symbol contract
//     changes. The loader rejects any plugin whose abi_version differs.
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

// Increment on any breaking change to the plugin ABI (see rules above).
#define QTUNE_PLUGIN_ABI_VERSION 1u

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
    uint32_t        abi_version;  // must equal QTUNE_PLUGIN_ABI_VERSION
    uint32_t        lvgl_version; // QTUNE_LVGL_VERSION at plugin build time
    QTunePluginType type;
    const char     *sdk_build;    // freeform SDK/build tag, for logging only
    const void     *interface;    // -> TunerGUIInterface* or TunerStandbyGUIInterface*
} QTunePluginDescriptor;

// Well-known exported symbol the loader looks up in each .so.
#define QTUNE_PLUGIN_DESCRIPTOR_SYMBOL "qtune_plugin_descriptor"

// The plugin SDK builds .so files with -fvisibility=hidden and --strip-all, so the
// descriptor must be explicitly given default visibility to survive into .dynsym
// where dlsym() can find it. Plugins declare the descriptor with this macro:
//   QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = { ... };
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
