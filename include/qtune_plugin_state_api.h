// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

// qtune_plugin_state_api.h
//
// Small, controlled persistent-storage surface for Q-Tune UI plugins. It lets a
// plugin save a little state that survives leaving the screen AND a full power
// cycle — e.g. a digital-pet standby ("Jamagotchi") that remembers its hunger,
// mood, and last-fed time. It is backed by the pedal's NVS (non-volatile storage)
// flash; plugins NEVER touch NVS, the filesystem, or flash directly — they go
// through these accessors, and the firmware owns namespacing, quota, and cleanup.
//
// SCOPE (where your data lives)
// -----------------------------
//   * Private (default): qt_state_*()  — scoped automatically to THIS plugin.
//     The firmware keys it by the plugin's descriptor `uid`, so two plugins can
//     use the same key names without colliding, and you never pass your uid.
//   * Shared (opt-in):   qt_state_shared_*()  — keyed by an author-chosen
//     namespace string so a tuner + standby PAIR can share ONE pet. Pick a
//     globally-unique, reverse-DNS namespace (e.g. "com.yourname.jamagotchi");
//     any plugin that knows the string can read/write it.
//
// !!! FLASH WEAR — THE ONE RULE THAT MATTERS !!!
// ----------------------------------------------
// NVS lives in flash, which wears out after a finite number of writes. NEVER
// commit on every frame. display_frequency() runs ~30x/second; committing there
// would destroy the flash in days. The model is:
//   * set_*()    — cheap; updates an in-RAM cache only.
//   * commit()   — EXPENSIVE; writes the cache to flash. Call it sparingly.
// In practice: mutate freely in RAM, and commit() only at natural checkpoints —
// on a SLOW timer (tens of seconds / minutes) and/or when meaningful state
// actually changed. The firmware also auto-commits pending writes after your
// cleanup() returns, so leaving the screen never loses data. Treat commit() as
// "I really want this on disk now," not "save after each tick."
//
// LIFECYCLE & CLEANUP
// -------------------
//   * Load your state in init() (get_*); if a key is absent you get a clean
//     slate — always handle "first run."
//   * Data is keyed by your `uid` (or shared namespace). Per Hard rule #5, never
//     change your uid after publishing — doing so ALSO orphans this data.
//   * Deleting a plugin's .so reclaims its private data at the next boot. A
//     crash-DISABLED plugin (renamed .so.disabled) keeps its data — it is still
//     considered installed. Use Settings -> Plugin Data on the pedal to wipe a
//     still-installed plugin's data by hand.

#ifndef QTUNE_PLUGIN_STATE_API_H
#define QTUNE_PLUGIN_STATE_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Result of a state operation. Check it on writes; reads return a byte count.
typedef enum {
    QT_STATE_OK = 0,        // success
    QT_STATE_NOT_FOUND,     // no value stored under that key
    QT_STATE_NO_SPACE,      // this namespace's quota is exhausted
    QT_STATE_TOO_LARGE,     // value exceeds QT_STATE_VALUE_MAX
    QT_STATE_INVALID,       // bad key / namespace (empty, too long, etc.)
    QT_STATE_ERR,           // backend / flash error
} qt_state_result_t;

// Limits the firmware enforces. Keys/namespaces longer than the key cap are
// rejected (the firmware hashes long uids internally, but YOUR key names must
// fit). Sizes are conservative — design pet state to be small.
#define QT_STATE_KEY_MAX    32      // max plugin-facing key length (bytes, excl. NUL)
#define QT_STATE_NS_MAX     48      // max shared-namespace string length (bytes, excl. NUL)
#define QT_STATE_VALUE_MAX  4096    // max bytes per value
#define QT_STATE_QUOTA      8192    // max total bytes stored per namespace

// --- Private per-plugin state (auto-scoped to THIS plugin's uid) -------------

// Stage `len` bytes of `data` under `key` in this plugin's private store. Writes
// to a RAM cache only — call qt_state_commit() to persist. Returns QT_STATE_OK,
// or QT_STATE_TOO_LARGE / QT_STATE_NO_SPACE / QT_STATE_INVALID.
qt_state_result_t qt_state_set_blob(const char *key, const void *data, size_t len);

// Read the value stored under `key` into `out` (capacity `out_cap` bytes).
// Returns the number of bytes read (0..out_cap), or a NEGATIVE qt_state_result_t
// on error (e.g. -QT_STATE_NOT_FOUND when the key is absent). If the stored
// value is larger than out_cap it is truncated to out_cap bytes.
int32_t qt_state_get_blob(const char *key, void *out, size_t out_cap);

// True if a value is currently stored (in cache or flash) under `key`.
bool qt_state_has(const char *key);

// Remove `key` from this plugin's store (RAM cache; persisted on next commit).
qt_state_result_t qt_state_erase(const char *key);

// Flush this plugin's pending writes to flash. EXPENSIVE — see the flash-wear
// note above; do NOT call per frame. The firmware also auto-commits after your
// cleanup() returns.
qt_state_result_t qt_state_commit(void);

// --- Shared state (opt-in; explicit author-namespaced) -----------------------
// Same semantics as the private calls, but keyed by `ns` instead of your uid, so
// a tuner + standby pair can share one pet. `ns` must be a stable, globally-
// unique, reverse-DNS string (e.g. "com.yourname.jamagotchi"), <= QT_STATE_NS_MAX
// chars. There is no access control — keep your namespace unique to avoid clashes.

qt_state_result_t qt_state_shared_set_blob(const char *ns, const char *key,
                                           const void *data, size_t len);
int32_t           qt_state_shared_get_blob(const char *ns, const char *key,
                                           void *out, size_t out_cap);
qt_state_result_t qt_state_shared_erase(const char *ns, const char *key);
qt_state_result_t qt_state_shared_commit(const char *ns);

#ifdef __cplusplus
}
#endif

#endif // QTUNE_PLUGIN_STATE_API_H
