# phase — Q-Tune Tuner Plugin Sample

This project builds `phase.so`, a tuner UI plugin for Q-Tune in the style of a
classic mechanical / Peterson-type **strobe** tuner. It appears in the menu as
**"Phase"** (the firmware already has a built-in tuner named "Strobe"). It's one
of the bundled tuner samples (alongside [`gauge`](../gauge)) — copy it and build
on it.

## What it looks like

Instead of a needle, the display shows several horizontal bands of vertical
stripes that appear to scroll:

- **Flat** (cents < 0): the stripes drift to the **left**.
- **Sharp** (cents > 0): the stripes drift to the **right**.
- The further out of tune, the **faster** they move (a blur when way off).
- **Dead in tune**: the pattern **freezes** and snaps to the user's accent colour.

Each band scrolls at a slightly different rate (like the octave rows on a real
strobe), so a locked note produces a clear "everything stops at once" read.
While searching, the stripes are a calm neutral grey; the instant the note is
inside the in-tune window they turn the user's chosen accent colour and stop —
so "grey + moving = keep tuning", "colour + frozen = locked".

The layout is orientation-aware: portrait stacks the note above the strobe panel;
landscape puts the note on the left and the panel on the right. All geometry is
computed from the live `screen_width` / `screen_height` / `is_landscape` host
globals.

## Build

From the SDK repo root (Docker, no local ESP-IDF needed):

```sh
./docker-build.sh plugins/tuner/phase
```

> The first build pulls the pinned ESP-IDF Docker image (a few GB) and the LVGL /
> elf_loader components — expect several minutes. Later builds reuse the cache and
> finish in seconds.

Output: `build/phase.so` (the Docker build runs the validator automatically).

## Upload

1. Put Q-Tune in USB Drive Mode (hold the foot switch at power-on) and copy
   `build/phase.so` into the `/plugins` folder. Or upload it over Wi-Fi at
   `http://<device-ip>/plugins`. See [`docs/DEPLOY.md`](../../../docs/DEPLOY.md).
2. Restart the device.
3. Select "Phase" from Settings > Tuner > Style.
4. No guitar handy? Enable *Settings > Advanced > Developer Tools > Test Signal*
   to drive the strobe with a built-in sweep through the six guitar strings (each
   easing from off-pitch into tune). Turn it off again for live tuning.

## Plugin identity

The descriptor's `uid` is `"qtune.phase.gdam5c"` — the plugin's stable identity.
The firmware assigns the numeric menu slot dynamically at load. Never change a
`uid` after publishing or the user's saved selection is lost.

Everything this plugin calls is listed in
[`docs/ALLOWED_SYMBOLS.md`](../../../docs/ALLOWED_SYMBOLS.md).
