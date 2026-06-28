# Glossary

Plain-language definitions of the terms used across the Q-Tune SDK. You don't need
to know any of this to build a plugin with Claude Code — it's here for when you're
curious or reading the [technical reference](REFERENCE.md).

| Term | What it means |
|------|---------------|
| **Q-Tune** | The guitar-tuner pedal this SDK builds UIs for. It has a 240×320 touchscreen and runs ESP32-S3 firmware. |
| **Plugin** | Your custom screen, packaged as one file the pedal loads at startup. It's either a *tuner* or a *standby* screen. |
| **Tuner (plugin)** | The active display shown *while you tune* — it shows the note and how flat/sharp you are. |
| **Standby (plugin)** | The idle/screensaver screen shown when you're *not* tuning. |
| **`.so` / shared object** | The compiled plugin file. The same kind of loadable plugin format desktop apps use. You build it from your C++ source. |
| **Relocatable / position-independent** | A property of the `.so` that lets the pedal load it at any spot in memory at startup. |
| **Firmware** | The pedal's built-in software. It's closed-source; your plugin talks to it only through the published API. |
| **ELF loader** | The part of the firmware that reads a `.so` file and loads it. (ELF is the file format.) |
| **`dlsym()`** | How the loader looks up a function inside your plugin by name — it uses this to find your `qtune_plugin_entry()`. |
| **Descriptor** | A small block of data in your plugin that tells the firmware its type, versions, and identity. |
| **Entry function** | `qtune_plugin_entry()` — the one function the firmware calls to get your descriptor. Required. |
| **`uid`** | Your plugin's permanent identity string (e.g. `qtune.my-tuner.k7f2q9`). The pedal remembers the user's choice by this. Never change it after publishing. |
| **ABI** | "Application Binary Interface" — the binary contract between a plugin and the firmware. The versions must match or the pedal won't load the plugin. |
| **LVGL** | The graphics library you draw with (widgets, animations, styles). Pinned to version 9.2.2. |
| **Symbol / allowlist** | A "symbol" is a function name in the compiled file. A plugin may only call the firmware functions on the [allowlist](ALLOWED_SYMBOLS.md). |
| **Validator** | `tools/validate_plugin.py` — checks your built `.so` on your computer before you upload it, so problems show up early. |
| **Scaffolder** | `tools/new_plugin.py` — creates a new, uniquely-named project for you (with a fresh `uid` and prefix). |
| **NVS** | The pedal's small non-volatile storage. It remembers which UI you selected (by `uid`), and backs the `qt_state_*` store your plugin can use to save state across reboots. |
| **Cents** | A fine measure of pitch: how flat (−) or sharp (+) a note is. 0 = perfectly in tune; the pedal gives your plugin this each frame. |
| **In-tune window** | How close to 0 cents counts as "in tune" (the user sets this; default ±3 cents). |
| **Tuner chrome** | The standard tuner furniture a plugin should draw: the note/cents readout, the mute indicator, and the settings (gear) button. |
| **Orientation** | Portrait (240×320) or landscape (320×240). The player can switch; good plugins handle both. |
