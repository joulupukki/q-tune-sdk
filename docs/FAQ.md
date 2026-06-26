# Frequently Asked Questions

## Can I sell plugins I build with this SDK?

Yes. The SDK is licensed under [Apache-2.0](../LICENSE), and the plugins you build
are your own work — you may share them, give them away, or sell them. Plugins link
against LVGL (MIT) at build time; the SDK doesn't redistribute LVGL (see
[`NOTICE.md`](../NOTICE.md)).

## Do I need to install Python on Windows just to build a plugin?

No. The build (`docker-build.ps1`) runs entirely inside the pinned ESP-IDF
container, which already includes `python3` and `pyelftools`, and it validates the
`.so` for you. You only need host Python if you want to run the **scaffolder**
(`tools/new_plugin.py`) or the **standalone validator** outside Docker — and on
Windows that's `python` / `py -3`, not `python3`.

## Does building work on Windows without WSL?

Yes. With [Docker Desktop](https://docs.docker.com/get-docker/) running, use
`.\docker-build.ps1 <project>` from PowerShell. The SDK and project are mounted at
fixed container paths, so Windows drive letters and backslashes don't matter.
Building inside a WSL2 shell with `docker-build.sh` also works if you prefer it.
Make sure your project lives on a drive Docker Desktop is allowed to share
(*Settings → Resources → File sharing*; your user profile is shared by default).

## Why can't my plugin use GPIO, NVS storage, Wi-Fi, the SD card, or raw audio/FFT?

This is a deliberate API boundary, not a missing feature. Plugins are unsandboxed
native code loaded into the firmware, so the host exposes a **small, stable
surface**: the LVGL drawing API, pitch data (frequency, note, octave, cents), user
settings, screen geometry, time, and randomness. Keeping the surface narrow is
what lets plugins keep working across firmware updates and keeps a buggy plugin
from bricking the pedal. If you think something essential is missing, ask on
[Discord](https://discord.gg/evtjkEj9GX).

## A function I want to call isn't in `ALLOWED_SYMBOLS.md`. Can I still use it?

No — if the firmware doesn't export it, the plugin won't load (the validator and
the loader both reject it). [`docs/ALLOWED_SYMBOLS.md`](ALLOWED_SYMBOLS.md) is the
authoritative list. Symbols can only be added on the firmware side; request one on
[Discord](https://discord.gg/evtjkEj9GX).

## Which SDK version matches my pedal's firmware?

See the [Compatibility Matrix](../COMPATIBILITY.md). Build with the row matching
the firmware on your device (Settings → About). The loader enforces the ABI and
LVGL contracts at boot and reports a mismatch on the pedal's `/plugins` page.

## Will my plugin work in both portrait and landscape?

It should — design it to. The pedal can run either orientation, so lay everything
out relative to the `screen_width` / `screen_height` / `is_landscape` globals
instead of hard-coding 240/320. Both bundled examples do this; copy their pattern.

## My plugin crashes the pedal when I leave the screen. Why?

Almost always a timer or animation that wasn't deleted. Delete every
`lv_timer` / `lv_anim` you create in your `cleanup()` callback — a timer that fires
after the screen is gone crashes the firmware. See
[`docs/TROUBLESHOOTING.md`](TROUBLESHOOTING.md).
