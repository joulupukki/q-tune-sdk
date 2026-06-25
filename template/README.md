# Plugin templates

Minimal, buildable starting points for your own Q-Tune plugin. You normally don't
copy these by hand — the scaffolding tool does it for you and fills in a unique
name, ID, and function prefix:

```sh
python3 tools/new_plugin.py --name "My Cool Tuner" --type tuner
python3 tools/new_plugin.py --name "My Standby"    --type standby
```

That produces a complete project you can build immediately:

```sh
./docker-build.sh path/to/my_cool_tuner
python3 tools/validate_plugin.py path/to/my_cool_tuner/build/my_cool_tuner.so
```

## What's here

- `tuner/` — a starter **tuner** UI: shows the detected note (using the built-in
  note artwork) and the cents offset, and tints to the user's accent colour when
  in tune. Implements the full `TunerGUIInterface`.
- `standby/` — a starter **standby** screen: a label that drifts and bounces with
  an `lv_timer`, tints when a note is detected, and jumps to where you touch the
  screen. Implements the full `TunerStandbyGUIInterface`, and shows the two things
  every standby plugin must get right: **deleting its timer in `cleanup()`** and
  reacting to **touch**.

Both are deliberately small. Keep the descriptor/entry boilerplate and the
interface wiring; change the drawing in `init()` / `display_frequency()` (and the
timer callback for standby) to build your own look.

## The placeholder tokens

These files contain `@TOKEN@` placeholders (`@PROJECT_NAME@`, `@PREFIX@`,
`@DISPLAY_NAME@`, `@PLUGIN_UID@`, `@SDK_BUILD_TAG@`, `@QTUNE_SDK_DIR@`) that the
scaffolding tool replaces. The raw template does **not** compile as-is — generate
a project with `new_plugin.py` instead of building this directory directly.

## Manual use (advanced)

If you'd rather copy by hand: copy `tuner/` or `standby/`, then replace every
`@TOKEN@` across all files, rename `main/plugin.cpp` to `main/<your_name>.cpp`,
and set `@QTUNE_SDK_DIR@` to the absolute path of your `q-tune-sdk` checkout.
