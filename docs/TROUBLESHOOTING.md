# Troubleshooting Your Q-Tune Plugin

This guide covers common build, validation, deployment, and runtime problems and how to fix them.

## Build errors

| Error message | Cause | Fix |
|---|---|---|
| `CMakeCache.txt … different directory` | Stale build cache after moving the project | `rm -rf build/` then rebuild with `./docker-build.sh` or `idf.py build` |
| `error: undefined reference to lv_xxx` | You called an LVGL function that isn't exported | Check `docs/ALLOWED_SYMBOLS.md`; replace with an exported equivalent (e.g., use `lv_scale_*` for gauges instead of `lv_arc_set_value`) |
| `Docker not found` / `docker: command not found` | Docker isn't installed or not in your PATH | [Install Docker](https://docs.docker.com/get-docker/); restart your terminal after install |
| Build runs but produces no `.so` file | CMake target name mismatch | Ensure your `CMakeLists.txt` `qtune_project_so()` name matches your actual plugin filename |
| `error: expected…` (C++ syntax) | Typo or missing bracket in your code | Check the line number; look for unclosed braces, missing semicolons, typos in function names |

## Validation errors

Run before uploading:

```sh
python3 tools/validate_plugin.py build/<your-plugin>.so
```

| Error message | Cause | Fix |
|---|---|---|
| `undefined symbol lv_xxx not in allowlist` | You called an unexported LVGL function | Replace with an allowed function from `docs/ALLOWED_SYMBOLS.md` |
| `missing symbol qtune_plugin_entry` | Entry function not exported or misspelled | Ensure your code exports both the `qtune_plugin_descriptor` data object and the `qtune_plugin_entry()` function with `QTUNE_PLUGIN_EXPORT` and `extern "C"` |
| `ABI major version mismatch` | Built against wrong SDK version | Use this repo's headers unchanged; don't mix headers from different SDK versions |
| `LVGL version mismatch` | `idf_component.yml` not pinned to `==9.2.2` | Edit `idf_component.yml`, set `lvgl/lvgl: "==9.2.2"`, delete `build/`, rebuild |
| `CONFIG_LV_COLOR_DEPTH mismatch` | Color depth in `sdkconfig.defaults` doesn't match firmware | Ensure `CONFIG_LV_COLOR_DEPTH=16` in your `sdkconfig.defaults`, delete `build/`, rebuild |
| `Descriptor not found` | Plugin binary is corrupt or not a valid ELF file | Rebuild from scratch; if it persists, check your `.cpp` file is actually being compiled |

## Deployment issues

### Plugin doesn't appear in the menu after restart

| Cause | Fix |
|---|---|
| Plugin wasn't restarted after upload | Power-cycle the pedal or go to Settings → Restart |
| Plugin has a missing or invalid `uid` | Every plugin needs a stable `uid` string in its descriptor. The scaffolding tool (`new_plugin.py`) generates one automatically; if you hand-edited it, make sure it's set and is **not** a bare integer (that space is reserved for built-ins). The firmware assigns the numeric slot for you — you don't choose a number |
| Two installed plugins share the same `uid` | This happens if you copied a plugin's files without changing its `uid` — the firmware loads only one of them. Give each its own uid (re-running `new_plugin.py` generates a fresh one). Check the `/plugins` web page to see what's installed |
| Plugin validation failed but was uploaded anyway | Visit `http://<pedal-ip>/plugins` and check if the plugin is listed as disabled. See "Crash-disabled plugins" below |
| Plugin loads but wrong interface type | You built a Tuner but looked in Settings → Standby Screen (or vice versa). Check which kind you implemented |

### I forgot the pedal's IP address

1. On the pedal, go to **Settings → Wi-Fi**.
2. Make sure you're connected.
3. The IP address is shown on the screen.

If the pedal isn't on Wi-Fi, connect it first.

### I can't upload via Wi-Fi but USB works

- Pedal may not be on Wi-Fi. Go to Settings → Wi-Fi and connect.
- Your computer and pedal may be on different networks. Connect both to the same Wi-Fi.
- Try `ping <device-ip>` from your computer to verify connectivity.

### USB Drive Mode won't mount

1. Make sure you're holding the foot switch **before** powering on, not after.
2. Hold it for 2+ seconds after power-on, then release.
3. The `/data` partition should appear as a USB device on your computer.
4. If it doesn't, eject the pedal, wait 5 seconds, and try again.

## Runtime issues

### Plugin crashes when I select it

**Cause**: The plugin has a runtime bug (usually a crash-causing issue like an undeleted timer, memory corruption, or dereferencing a null pointer).

**What happens**: The firmware detects the crash, logs it to the serial monitor, and increments a crash counter for that plugin. After 2 crashes, the plugin is quarantined (disabled).

**What to do**:

1. Check the serial monitor for error output (see "Reading serial output" below).
2. Look for common causes in the table below.
3. Edit your plugin code to fix the issue.
4. Rebuild, revalidate, re-upload, and try again.

If the plugin is crash-quarantined (`.so.disabled`), see "Re-enabling crash-disabled plugins" below.

### Pedal keeps rebooting / crashes on boot

**Cause**: Your plugin crashes during `init()` before Wi-Fi comes up, so you can't access the `/plugins` page to delete it.

**Fix**: Use Safe Mode to bypass all plugins and recover:

1. Power off the pedal.
2. Hold the **BOOT button (GPIO0)** — this is the small recessed button on the side or bottom of the pedal.
3. While holding BOOT, power on the pedal.
4. Keep holding for ~2 seconds after the screen comes on, then release.
5. The pedal boots with all plugins disabled and shows only built-in UIs.
6. Go to **Settings → Wi-Fi** and connect.
7. Open `http://<device-ip>/plugins` and delete or replace the bad plugin.
8. Power-cycle (no BOOT button this time) to resume normal operation.

If you don't know which button is BOOT, refer to your pedal's documentation or try the small unlabeled button on the side.

### Plugin doesn't respond to touchscreen

**Cause**: You didn't add touch event callbacks, or the callback isn't registered to the right object.

**Fix**: See `docs/TOUCH.md` for a complete example. In brief:

```c
// Inside init(screen):
lv_obj_add_event_cb(screen, on_tap, LV_EVENT_PRESSED, NULL);

// Callback:
static void on_tap(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    // p.x and p.y are the touch coordinates
}
```

### Colors look wrong / inverted / washed out

**Cause**: Color depth mismatch. The firmware uses 16-bit RGB565, and your plugin must too.

**Fix**: 

1. In `sdkconfig.defaults`, confirm `CONFIG_LV_COLOR_DEPTH=16`.
2. In `idf_component.yml`, confirm `lvgl/lvgl: "==9.2.2"`.
3. Delete `build/` and rebuild.

### Display updates are too slow or jerky

**Cause**: Your `display_frequency()` callback is doing too much work (> 1 ms).

**Fix**:

1. Cache the previous values. Only update LVGL objects if the value changed significantly.
2. See `examples/example_tuner` for the caching pattern.
3. Avoid expensive operations in `display_frequency()` (don't call `malloc`, don't iterate over large data structures).

### Display doesn't update when I change a user setting (e.g., reference frequency)

**Cause**: You read the setting once in `init()` but didn't check again in `display_frequency()`.

**Fix**: Call `qt_get_reference_frequency()`, `qt_get_note_name_palette()`, etc. inside `display_frequency()` so you pick up changes live.

### Plugin works on its own but crashes when I switch between plugins

**Cause**: Your `cleanup()` callback isn't properly freeing resources (usually timers or animations).

**Fix**:

1. Ensure you call `lv_timer_delete()` for every timer you created.
2. Do NOT call `lv_obj_del()` on children of the `screen` you were given — the host cleans them up for you.
3. See "Timer lifecycle" in `examples/example_standby/README.md`.

### Re-enabling crash-disabled plugins

If your plugin has crashed twice, it's quarantined as `<name>.so.disabled`. To re-enable it:

1. Fix the bug in your code.
2. Rebuild, revalidate, re-upload (use the same filename; it will replace the `.so` file).
3. Open `http://<pedal-ip>/plugins` in your browser.
4. Find the plugin in the list (it shows as disabled).
5. Click **Re-enable**.
6. Restart the pedal.
7. Your fixed plugin loads on boot.

If it crashes again, repeat the process or investigate the crash logs (see below).

## Debugging with serial output

Your plugin can print to the serial monitor using `printf()`:

```cpp
printf("Debug: frequency = %f Hz\n", frequency);
```

### How to read serial output

**Option 1: Monitor over USB**

If your pedal is connected to your computer via USB:

```sh
# macOS / Linux
screen /dev/tty.usbserial-* 115200

# Windows
# Use PuTTY or another serial terminal; device is COMx at 115200 baud
```

Press Ctrl+C to exit.

**Option 2: ESP-IDF built-in monitor**

If you built with native ESP-IDF (not Docker):

```sh
idf.py monitor
```

Look for:
- Crash stack traces (ESP-IDF logs these).
- Your plugin's `printf()` output.
- Firmware plugin-loader messages (ABI/LVGL checks).

**What to look for**:

- Your own `printf()` calls (e.g., "Debug: entered init").
- Firmware messages like `Plugin loaded: <name> (ID 123)`.
- Crash reasons: `Guru Meditation Error`, `Segmentation fault`, `illegal instruction`.

If you see a crash, the stack trace usually hints at the culprit. Common patterns:

- `abort()` called → likely a crash counter incremented.
- Null pointer dereference (access to `0x00000000`) → you dereferenced a pointer that wasn't initialized.
- Undefined instruction → you called a function that doesn't exist or the binary is corrupt.

## Common code bugs

### Timer crashes the firmware after cleanup

**Symptom**: Plugin works, but selecting a different plugin crashes.

**Cause**: You didn't delete a timer in `cleanup()`.

**Fix**: In your `cleanup()` function, call `lv_timer_delete()` for every `lv_timer_create()` you made:

```c
static lv_timer_t *s_timer = NULL;

void my_init(lv_obj_t *screen) {
    s_timer = lv_timer_create(my_timer_cb, 50, NULL);
}

void my_cleanup(void) {
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
}
```

### NOTE_NONE crashes or draws wrong

**Cause**: You assumed `note_name` is always valid, but it's `NOTE_NONE` when no pitch is detected.

**Fix**: Always handle `NOTE_NONE`:

```c
if (note_name == NOTE_NONE) {
    lv_label_set_text(label, "--");
} else {
    const char *note_str = name_for_note(note_name);
    lv_label_set_text_fmt(label, "%s%d", note_str, octave);
}
```

### Using unexported LVGL functions

**Symptom**: Validator passes but plugin doesn't load on the pedal (silent failure).

**Cause**: You used an `lv_*` function that's in LVGL's headers but not exported by the firmware.

**Fix**: Always validate before uploading. If you missed an unexported function, the validator will catch it. Replace it with an exported equivalent from `docs/ALLOWED_SYMBOLS.md`.

### Color doesn't use user's accent palette

**Symptom**: Plugin always uses a hardcoded color instead of the user's accent color.

**Cause**: You didn't call `qt_get_note_name_palette()`.

**Fix**:

```c
lv_palette_t palette = qt_get_note_name_palette();
lv_color_t accent = lv_palette_main(palette);
lv_obj_set_style_bg_color(obj, accent, 0);
```

## Getting help

- **Validator errors**: Read the error message carefully; it usually says exactly which symbol is missing. Check `docs/ALLOWED_SYMBOLS.md`.
- **Build failures**: Look at the compiler output. Search the error message in the README or this document.
- **Runtime crashes**: Check the serial monitor for a stack trace (see "Debugging with serial output" above). Use Safe Mode to recover.
- **Still stuck**: See `README.md` for the full technical reference, or check `examples/example_tuner` / `examples/example_standby` for working implementations.

## Quick recovery checklist

**Plugin crashes on boot (Safe Mode needed):**
- [ ] Power off pedal.
- [ ] Hold BOOT button (GPIO0).
- [ ] Power on while holding.
- [ ] Release after 2 seconds.
- [ ] Connect to Wi-Fi (Settings).
- [ ] Visit `http://<ip>/plugins` and delete/replace plugin.
- [ ] Power-cycle normally.

**Plugin crashes after loading (crash-quarantined):**
- [ ] Fix the bug in your code.
- [ ] Rebuild and revalidate.
- [ ] Re-upload (same filename).
- [ ] Open `http://<ip>/plugins` and click **Re-enable**.
- [ ] Restart pedal.

**Stale build cache:**
- [ ] `rm -rf build/`
- [ ] Rebuild.
