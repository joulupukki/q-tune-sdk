# Contributing

Thanks for your interest in the Q-Tune plugin SDK! This repo is the **toolkit for
building UI plugins** (tuners and standby screens) for the Q-Tune guitar-tuner
pedal. Here's how to get involved depending on what you want to do.

## I'm building a plugin

You don't need to contribute to this repo at all — just use it. See
[`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) and the
[`examples/`](examples/). Plugins you build are yours: the SDK is Apache-2.0 and
you're free to share or sell what you make (see [FAQ](docs/FAQ.md)).

## I found a bug or have a question

- **SDK bugs** (build scripts, scaffolder, validator, docs, headers): open an
  issue on this repository with steps to reproduce, your OS, and the validator /
  build output.
- **Usage questions** and show-and-tell: join the community on
  [Discord](https://discord.gg/evtjkEj9GX).

## I need a host symbol that isn't exported

The allowlist in [`docs/ALLOWED_SYMBOLS.md`](docs/ALLOWED_SYMBOLS.md) is generated
from the **firmware's** export table — the SDK can't add a symbol on its own. If a
function you need isn't exported, request it on
[Discord](https://discord.gg/evtjkEj9GX) or in the
[firmware project](https://github.com/btimothy/q-tune). Once it's exported in a
firmware release, a matching SDK release picks it up.

## I want to send a pull request to the SDK

Improvements to the build tooling, docs, examples, and templates are welcome.

- **Keep it cross-platform.** The build path must work on macOS, Linux, and
  Windows (via Docker Desktop). Don't reintroduce host-path assumptions into the
  build scripts.
- **Line endings: LF.** A [`.gitattributes`](.gitattributes) enforces this. Don't
  commit CRLF in shell scripts, Python, CMake, or sources (PowerShell `.ps1` files
  are the one exception and use CRLF).
- **Test what you touch.** If you change a build script or the validator, build
  both examples (`./docker-build.sh examples/example_tuner` and
  `examples/example_standby`) and confirm the auto-validation passes.
- **Match the surrounding style.** Plain language in docs; keep example plugins
  idiomatic and landscape-aware (lay out relative to `screen_width` /
  `screen_height`, never hard-coded 240/320).
- License: contributions are accepted under [Apache-2.0](LICENSE).

By participating you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).
