// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/*
 * stub_app.c
 *
 * Stub entry point for the (unused) normal ESP-IDF application image.
 *
 * `idf.py build` always links a normal app, which requires an app_main(). The
 * real deliverable is gauge.so, produced by qtune_project_so() in the
 * top-level CMakeLists.txt. The plugin source (gauge.cpp) is therefore
 * intentionally NOT registered with this component — it references host firmware
 * symbols (qt_get_*, screen_width, …) that are undefined in a standalone app
 * link and are only resolved at load time inside the firmware. qtune_project_so()
 * picks up gauge.cpp by globbing this component directory.
 */
void app_main(void) {}
