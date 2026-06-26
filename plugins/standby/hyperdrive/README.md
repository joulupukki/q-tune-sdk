# hyperdrive — Q-Tune Standby Plugin Sample

This project builds `hyperdrive.so`, a standby UI plugin for Q-Tune: a warp-speed
starfield screensaver. It's one of the bundled standby samples (alongside
[`bouncer`](../bouncer)) — copy it and build on it.

## What it looks like

Stars streak outward from a central vanishing point, accelerating and lengthening
into light-trails as they rush past — the classic "jump to hyperspace" effect.

- **Tap anywhere** and the field surges into a warp boost that eases back down.
- **Play a note** and the stars take on the user's accent colour (and push a
  little faster) — so strumming literally drives the ship.
- **Each note struck** also launches its letter out of the vanishing point toward
  you — an "asteroid" that fades as it flies. With the pedal monitoring in
  buffered-bypass mode it doubles as a glanceable read of the pitch it's hearing.

Each star is an `lv_line` whose two endpoints are recomputed every frame, so the
trails get longer toward the edges for a real sense of speed. It is orientation
aware (240×320 / 320×240): the vanishing point and travel limits are re-read from
the live screen geometry every frame, so the field re-centres if the screen
rotates. It uses one `lv_timer`, deleted in `cleanup()`.

## Build

From the SDK repo root (Docker, no local ESP-IDF needed):

```sh
./docker-build.sh plugins/standby/hyperdrive
```

Output: `build/hyperdrive.so` (the Docker build runs the validator automatically).

## Upload

1. Put Q-Tune in USB Drive Mode (hold the foot switch at power-on) and copy
   `build/hyperdrive.so` into the `/plugins` folder. Or upload it over Wi-Fi at
   `http://<device-ip>/plugins`. See [`docs/DEPLOY.md`](../../../docs/DEPLOY.md).
2. Restart the device.
3. Select "Hyperdrive" from Settings > Standby Screen.

## Plugin identity

The descriptor's `uid` is `"qtune.hyperdrive.vxggzt"` — the plugin's stable
identity. The firmware assigns the numeric menu slot dynamically at load. Never
change a `uid` after publishing or the user's saved selection is lost.

## Important: timer lifecycle

The plugin creates an `lv_timer` in `init()` and **must** delete it in `cleanup()`
via `lv_timer_delete()` — a timer that fires after the screen is gone crashes the
firmware. Do NOT call `lvgl_port_lock()` / `lvgl_port_unlock()`: the firmware
already holds the LVGL lock during every plugin callback.

Everything this plugin calls is listed in
[`docs/ALLOWED_SYMBOLS.md`](../../../docs/ALLOWED_SYMBOLS.md).
