# Touch Input: Making Your Plugin React to Taps

The Q-Tune pedal has a capacitive touchscreen. This guide explains how to add tap-responsive behavior to your plugin.

## The basics

To respond to a touch, you:

1. Create an event callback function.
2. Register that callback on an LVGL object (usually the screen) with `lv_obj_add_event_cb()`.
3. Inside the callback, get the touch point and react to it.

All three of these functions are allowlisted and ready to use.

## Minimal example

```cpp
// This callback fires when the screen is touched.
static void on_screen_tap(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    
    // p.x and p.y are the touch coordinates (0, 0) is top-left.
    printf("Tapped at x=%d, y=%d\n", p.x, p.y);
    
    // React here: change color, update state, etc.
}

// Inside your init(lv_obj_t *screen) function:
void my_init(lv_obj_t *screen) {
    // ... create your objects ...
    
    // Register the tap handler on the screen.
    lv_obj_add_event_cb(screen, on_screen_tap, LV_EVENT_PRESSED, NULL);
}
```

## Event types

Two main touch events are available:

| Event | When it fires | Use for |
|-------|---------------|---------|
| `LV_EVENT_PRESSED` | When a finger touches down | Immediate response to a tap (feels snappier) |
| `LV_EVENT_CLICKED` | When a finger lifts up after a tap | Confirming an action; more deliberate |

For most cases, use `LV_EVENT_PRESSED`. For deliberate actions (like switching modes), use `LV_EVENT_CLICKED`.

## Getting the touch point

Inside your event callback:

```cpp
static void on_tap(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    
    // p.x: x-coordinate (0 to screen_width - 1)
    // p.y: y-coordinate (0 to screen_height - 1)
}
```

Use `p.x` and `p.y` to determine where the user tapped.

## Reacting to touch: three ideas

### Idea 1: Tap to change color

Toggle the plugin's color when tapped:

```cpp
static lv_color_t s_color = LV_COLOR_MAKE(0, 255, 0); // Start green

static void on_tap(lv_event_t *e) {
    if (s_color.full == 0x00FF00) {
        // Currently green, switch to blue
        s_color = lv_color_hex(0x0000FF);
    } else {
        // Currently blue, switch back to green
        s_color = lv_color_hex(0x00FF00);
    }
    
    // Redraw the relevant object (or let display_frequency update it next frame)
    lv_obj_invalidate(screen);
}

void my_init(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, on_tap, LV_EVENT_CLICKED, NULL);
}
```

### Idea 2: Tap to cycle through modes

Cycle between different display modes (e.g., meter → strobe → waveform):

```cpp
static uint8_t s_mode = 0; // 0 = meter, 1 = strobe, 2 = waveform

static void on_tap(lv_event_t *e) {
    s_mode = (s_mode + 1) % 3;
    
    // In the next display_frequency call, check s_mode and render accordingly
    printf("Switched to mode %d\n", s_mode);
}

void my_display_frequency(float frequency, float target_frequency,
                          TunerNoteName note_name, int octave, float cents,
                          bool show_mute_indicator) {
    if (s_mode == 0) {
        // Render meter mode
    } else if (s_mode == 1) {
        // Render strobe mode
    } else {
        // Render waveform mode
    }
}

void my_init(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, on_tap, LV_EVENT_PRESSED, NULL);
}
```

### Idea 3: Tap location spawns something

Use the tap coordinates to spawn objects at that location (e.g., particles, highlights):

```cpp
static void on_tap(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    
    // Create a small circle at the tap point
    lv_obj_t *circle = lv_obj_create(lv_screen_active());
    lv_obj_set_size(circle, 20, 20);
    lv_obj_set_pos(circle, p.x - 10, p.y - 10);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0xFF00FF), 0);
    
    // Optional: delete it after a short time
    // (requires careful timer management; see TROUBLESHOOTING.md)
}

void my_init(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, on_tap, LV_EVENT_PRESSED, NULL);
}
```

## Tapping specific objects

You can register callbacks on any LVGL object, not just the screen:

```cpp
void my_init(lv_obj_t *screen) {
    // Create a button
    lv_obj_t *btn = lv_obj_create(screen);
    lv_obj_set_size(btn, 100, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    
    // Only this button fires the callback when tapped
    lv_obj_add_event_cb(btn, on_button_tap, LV_EVENT_CLICKED, NULL);
}

static void on_button_tap(lv_event_t *e) {
    printf("Button tapped!\n");
}
```

## Example: the bouncer sample demonstrates touch

The `plugins/standby/bouncer` project implements touch interactivity end-to-end. It's a bouncing dot that changes color when tapped. Refer to its source code for a complete, working reference.

## Unregistering a callback

If you need to stop responding to touches (rare), you can remove a callback:

```cpp
lv_obj_remove_event_cb(obj, on_tap);
```

However, it's simpler to just leave the callback registered and ignore the event inside if a flag is set.

## Notes on performance

- Event callbacks fire immediately when a touch is detected.
- Avoid heavy computation inside a tap callback (keep it fast, like other LVGL callbacks).
- If you create objects in the callback, clean them up in `cleanup()` if they have associated timers.

## Touchscreen limitations

- The touchscreen detects pressure over the entire screen, but you can simulate button regions by checking `p.x` and `p.y` against expected bounds.
- There is no multi-touch support (only single finger).
- There is no gesture recognition (swipes, pinches, etc.) — only taps.

## If touch isn't working

- Ensure you're registering the callback on an object before `init()` returns.
- Make sure you're using `LV_EVENT_PRESSED` or `LV_EVENT_CLICKED`, not another event type.
- Confirm the object hasn't been deleted in `cleanup()` before `init()` exits.
- Check the serial monitor for error output (see `docs/TROUBLESHOOTING.md` for how to read serial output).

## Full working example

Here's a minimal tuner UI that cycles mode on tap:

```cpp
#include "qtune_plugin.h"

static lv_obj_t *s_screen = NULL;
static uint8_t s_mode = 0;

static const char *et_get_name(void) { return "Tap Tuner"; }
static TuningUIType et_get_type(void) { return TuningUITypeStandard; }

static void on_tap(lv_event_t *e) {
    s_mode = (s_mode + 1) % 2;
    printf("Mode switched to %d\n", s_mode);
    lv_obj_invalidate(s_screen);
}

static void et_init(lv_obj_t *screen) {
    s_screen = screen;
    
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Tap to switch mode");
    lv_obj_center(label);
    
    lv_obj_add_event_cb(screen, on_tap, LV_EVENT_PRESSED, NULL);
}

static void et_display_frequency(float frequency, float target_frequency,
                                  TunerNoteName note_name, int octave,
                                  float cents, bool show_mute_indicator) {
    if (s_mode == 0) {
        // Mode 0: show note name
    } else {
        // Mode 1: show cents
    }
}

static void et_align_settings_button(lv_obj_t *btn) {
    lv_obj_align_to(btn, s_screen, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
}

static void et_cleanup(void) {
    s_screen = NULL;
}

static TunerGUIInterface s_interface = {
    .get_name = et_get_name,
    .get_type = et_get_type,
    .init = et_init,
    .display_frequency = et_display_frequency,
    .align_settings_button = et_align_settings_button,
    .cleanup = et_cleanup,
};

extern "C" {
QTUNE_PLUGIN_EXPORT QTunePluginDescriptor qtune_plugin_descriptor = {
    .abi_version = QTUNE_PLUGIN_ABI_VERSION,
    .lvgl_version = QTUNE_LVGL_VERSION,
    .type = QTUNE_PLUGIN_TUNER,
    .sdk_build = "tap-tuner-1.0",
    .interface = &s_interface,
    .uid = "qtune.tap-tuner.0001",   // stable identity; firmware assigns the number
};

QTUNE_PLUGIN_EXPORT const QTunePluginDescriptor *qtune_plugin_entry(void) {
    return &qtune_plugin_descriptor;
}
}
```

Compile, validate, upload, select in Settings → Tuner UI, and tap the screen to cycle modes.
