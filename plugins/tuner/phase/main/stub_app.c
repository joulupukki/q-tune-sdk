/*
 * stub_app.c — stub entry point for the (unused) normal ESP-IDF app image.
 *
 * `idf.py build` always links a normal app, which requires an app_main(). The
 * real deliverable is phase.so, produced by qtune_project_so() in the
 * top-level CMakeLists.txt. The plugin source is NOT registered with this
 * component — it references host firmware symbols resolved only at load time.
 */
void app_main(void) {}
