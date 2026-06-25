# Getting Started: Build Your First Q-Tune Plugin

This guide walks you through building a custom UI plugin for the Q-Tune guitar-tuner pedal without writing code yourself. You'll describe what you want in plain English, let Claude Code do the heavy lifting, and end up with a `.so` file ready to load on your pedal.

## What you'll need

- **Docker** ([install here](https://docs.docker.com/get-docker/)) — the easiest way to build. The Docker container has all the compiler tools pinned and ready to go.
- **The Q-Tune SDK** — clone or download this repository.
- **Claude Code** (or any text editor and a terminal, but Claude Code is much easier for non-programmers).

## The big picture

Your plugin will:

1. **Get scaffolded** — a tool generates a uniquely-named project folder with all the boilerplate in place.
2. **Get coded** — Claude Code writes or edits the C++ file based on your description.
3. **Get built** — Docker compiles it to a `.so` shared object.
4. **Get validated** — a Python script checks for common mistakes before upload.
5. **Get uploaded** — you copy it to your pedal via Wi-Fi or USB.
6. **Get selected** — you pick it in the pedal's settings menu.

## Step 1: Install Docker

If you don't have Docker yet, [follow the official installation guide](https://docs.docker.com/get-docker/). It takes 5 minutes. Once installed, you should be able to run `docker --version` in a terminal.

**On Windows?** Everything here works on Windows with [Docker Desktop](https://docs.docker.com/get-docker/) — no WSL setup needed. The only difference is the build command: use `.\docker-build.ps1 <project>` (PowerShell) instead of `./docker-build.sh`, and `python` instead of `python3` for the scaffolding tool. If you're using Claude Code, it will pick the right commands for you.

## Step 2: Get the SDK

Clone the Q-Tune SDK repository or download it as a ZIP. Open a terminal in the SDK folder:

```sh
cd /path/to/q-tune-sdk
```

You'll need this directory open in Claude Code.

## Step 3: Decide what you want

Before you start, think about:

- **Tuner or standby?**
  - A **tuner** UI is what shows while you're tuning (needle, meter, strobe, etc.). It gets active pitch data at 30 Hz.
  - A **standby** UI is the idle screen shown when you're not tuning (screensaver, info display, etc.).
  - You can build one or both, but each is a separate plugin.

- **What should it look like?** Be specific but friendly.
  - ✓ "A big glowing needle with the note name in the middle"
  - ✓ "A bouncing colorful ball that reacts to pitch"
  - ✓ "A simple meter bar with green-to-red when out of tune"

## Step 4: Open the SDK in Claude Code

1. Open Claude Code.
2. Go to **File → Open Folder** and select the `q-tune-sdk` directory.
3. You'll see the folder structure in the sidebar (examples, docs, include, tools, etc.).

## Step 5: Ask Claude Code to build your plugin

Use Claude Code's chat or context to describe what you want. Here's a template prompt:

```
I want to build a [tuner/standby] UI plugin for Q-Tune.
Description: [your description]

Please:
1. Use the scaffolding tool (python3 tools/new_plugin.py) to create a new project with a unique name.
2. Write the C++ code for the plugin, implementing the required interface.
3. Build it with docker-build.sh.
4. Validate it with tools/validate_plugin.py.
5. Show me the final path to the .so file.
```

For example:

```
I want to build a tuner UI plugin for Q-Tune that shows a big glowing needle 
and the note name in large text in the middle of the screen. The needle should 
sweep left-to-right as the pitch goes flat to sharp, and turn green when in tune.

Please:
1. Use the scaffolding tool to create a new project with a unique name.
2. Write the C++ code implementing TunerGUIInterface.
3. Build it with docker-build.sh.
4. Validate it with tools/validate_plugin.py.
5. Show me the final path to the .so file.
```

Claude Code will:

- Call `python3 tools/new_plugin.py` to create a new project with a stable auto-generated uid (the plugin's identity), name prefix, and build tag. You don't pick a number — the firmware assigns one dynamically at load time.
- Write the C++ source code implementing your tuner or standby interface.
- Run `./docker-build.sh <project-path>` to build it. Docker will download the pinned ESP-IDF and LVGL and compile your code.
- Run `python3 tools/validate_plugin.py <path-to-.so>` to check it before declaring success.
- Show you the path to your `.so` file (usually something like `projects/my_tuner/build/my_tuner.so`).

If the build or validation fails, Claude Code will show the error and suggest a fix. Follow the troubleshooting hints in `docs/TROUBLESHOOTING.md` if needed.

## Step 6: Get your .so file

Once Claude Code finishes, you'll have a built plugin file. Claude Code will tell you its exact path. Download or copy it to your computer.

## Step 7: Upload to your pedal

Follow the steps in `docs/DEPLOY.md`:

- **Option A (Wi-Fi)**: Connect your pedal to Wi-Fi, open `http://<pedal-ip>/plugins` in a browser, and upload your `.so` file.
- **Option B (USB)**: Hold the foot switch at power-up to mount the pedal's storage as a USB drive, and drag your `.so` file into the `/plugins` folder.

## Step 8: Activate your plugin

After uploading, **restart your pedal** (power-cycle or Settings → Restart). The firmware scans `/data/plugins/`, loads your `.so`, and registers it. Then go to:

- **Settings → Tuner UI** (if you built a tuner) or
- **Settings → Standby Screen** (if you built a standby)

Select your plugin from the list. You'll see it immediately.

## What to expect

If your plugin works:
- It appears in the menu under its custom name.
- It responds to pitch data and updates the display.
- It handles the touchscreen (if you added touch support).
- It survives a restart without crashes.

If something goes wrong:
- The plugin doesn't appear in the menu → check `docs/TROUBLESHOOTING.md`.
- The pedal crashes when you select it → the plugin may have a timer or memory leak. See "Troubleshooting" and "Crash recovery" in `docs/TROUBLESHOOTING.md`.
- Colors look wrong → see the color depth note in `docs/TROUBLESHOOTING.md`.

## Where to go next

- **Want to customize your plugin?** Edit the source file and rebuild. Claude Code can help you tweak colors, layout, behavior, etc.
- **Want to add touch interactivity?** See `docs/TOUCH.md` for how to respond to taps.
- **Want to dive deeper?** Read `README.md` (the technical reference), look at `examples/example_tuner` and `examples/example_standby`, and explore `docs/ALLOWED_SYMBOLS.md` for the full list of available LVGL widgets.
- **Having trouble?** See `docs/TROUBLESHOOTING.md` and `docs/DEPLOY.md`.

## Quick checklist

- [ ] Docker installed and working (`docker --version` succeeds).
- [ ] Q-Tune SDK cloned or downloaded.
- [ ] Claude Code open with the SDK folder.
- [ ] Asked Claude Code to scaffold and build your plugin.
- [ ] Plugin validated and built successfully.
- [ ] Uploaded to pedal via Wi-Fi or USB.
- [ ] Pedal restarted.
- [ ] Plugin selected in Settings menu.
- [ ] Plugin works and looks good.

Happy building!
