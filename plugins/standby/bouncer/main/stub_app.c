// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Boyd Timothy

/*
 * stub_app.c
 *
 * Stub entry point for the (unused) normal ESP-IDF application image. The real
 * deliverable is bouncer.so, produced by qtune_project_so() in the
 * top-level CMakeLists.txt. The plugin source (bouncer.cpp) is NOT
 * registered with this component — it references host firmware symbols that are
 * undefined in a standalone app link and are resolved at load time inside the
 * firmware. qtune_project_so() globs this directory to build the .so.
 */
void app_main(void) {}
