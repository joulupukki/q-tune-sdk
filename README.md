# q-tune-sdk

Build your own UI for the **[Q-Tune guitar tuner pedal](https://www.q-tune.com/)** (it runs on an ESP32-S3) — a custom tuning
display or a standby/screensaver screen — and load it onto the pedal. Your UI ships
as a small plugin file (a `.so`; see the [glossary](docs/GLOSSARY.md)) that the
pedal picks up at startup and lists right next to its built-in screens.

Licensed under **Apache-2.0** (see [`LICENSE`](LICENSE)) — you're free to build and
**sell** plugins made with this SDK.

---

## What you can build

Two kinds of plugin: a **tuner** (the active display while you tune) or a **standby
screen** (what the pedal shows when idle). The SDK ships five complete, working
samples to reference:

| Sample | Kind | What it looks like |
|--------|------|--------------------|
| [`gauge`](plugins/tuner/gauge) | Tuner | A round needle gauge that sweeps with how flat/sharp you are |
| [`phase`](plugins/tuner/phase) | Tuner | Scrolling strobe stripes that freeze the instant you're in tune |
| [`bouncer`](plugins/standby/bouncer) | Standby | A dot bouncing around the screen; tap to send it somewhere |
| [`hyperdrive`](plugins/standby/hyperdrive) | Standby | A warp-speed starfield that boosts when you tap or play |
| [`jamagotchi`](plugins/standby/jamagotchi) | Standby | An interactive digital pet that remembers its state across reboots (proof-of-concept for saving state) |

Anything you can draw with [LVGL](https://lvgl.io) (gauges, meters, animations,
pixel art, touch reactions) is fair game.

## Q-Tune at a glance

- **Screen:** 240×320 touchscreen, usable in **portrait or landscape** (the player
  chooses — good plugins support both).
- **What it gives your plugin each frame:** the detected note, how many cents
  flat/sharp it is, and the user's settings (accent color, reference pitch, …).
- **Two plugin kinds:** tuner UI and standby screen — one plugin is one kind.

---

## Choose your path

### I just want a custom screen (little or no coding)

Describe what you want in plain English and let [Claude Code](https://claude.com/claude-code)
write, build, and validate the plugin for you. You only need
[Docker](https://docs.docker.com/get-docker/) installed.

→ **Start with [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md).**

### I want to write the C++ myself

Scaffold a uniquely-named project, edit it, build, and validate — all with Docker,
no local toolchain needed:

```sh
python3 tools/new_plugin.py --name "My Tuner" --type tuner
./docker-build.sh plugins/tuner/my_tuner
python3 tools/validate_plugin.py plugins/tuner/my_tuner/build/my_tuner.so
```

The scaffolder drops your project in `plugins/<type>/` next to the samples (it's
gitignored, so your work stays separate and survives a `git pull`) and prints the
exact build/validate paths when it finishes.

The first `docker-build.sh` downloads the pinned ESP-IDF image (a few GB) plus
components, so it takes several minutes; every build after that reuses the cache and
finishes in seconds.

→ **Then read [`docs/REFERENCE.md`](docs/REFERENCE.md)** for the full contract,
lifecycle, and allowed API, and study the [samples](plugins/) and
[`template/`](template/).

### I'm using an AI agent / Claude Code

This repo ships a [`CLAUDE.md`](CLAUDE.md) that the assistant loads automatically —
it encodes every rule needed to one-shot a working plugin, using the samples
as references. Start by copying one of the samples or create a completely new idea. There's nothing else you need to set up.

---

## Where things live

| Doc | What it's for |
|-----|---------------|
| [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) | The no-code walkthrough (Claude Code + Docker) |
| [`docs/REFERENCE.md`](docs/REFERENCE.md) | Full technical reference (descriptor, ABI, API, lifecycle) |
| [`docs/GLOSSARY.md`](docs/GLOSSARY.md) | Plain-language definitions of the jargon |
| [`docs/DEPLOY.md`](docs/DEPLOY.md) | Upload to the pedal, restart, and select your plugin |
| [`docs/TOUCH.md`](docs/TOUCH.md) | Reacting to the touchscreen |
| [`docs/ALLOWED_SYMBOLS.md`](docs/ALLOWED_SYMBOLS.md) | The exact list of functions a plugin may call |
| [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) | Error → fix reference |
| [`docs/MONITOR.md`](docs/MONITOR.md) | Watch the pedal's serial console (zero install) |
| [`docs/FAQ.md`](docs/FAQ.md) | Licensing, Windows, API boundaries |
| [`COMPATIBILITY.md`](COMPATIBILITY.md) | SDK ↔ ABI ↔ LVGL ↔ firmware version matrix |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Improving the SDK itself (you don't need this to build plugins) |

---

> **Safety:** a plugin is native code that runs unsandboxed on your pedal. Only
> install plugins you built or whose source you trust — see [`DISCLAIMER.md`](DISCLAIMER.md).
> This SDK is provided "as is", without warranty; you use it at your own risk.
