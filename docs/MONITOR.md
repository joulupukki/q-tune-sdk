# Watching the serial console

When a plugin misbehaves — it won't load, the pedal reboots, colors are wrong —
the fastest way to see *why* is the pedal's **serial console**. The firmware
prints boot messages and `printf()` / `ESP_LOG` output there, including the exact
reason a plugin was rejected and any crash backtrace.

The console comes out of the pedal's **USB-C port** (the ESP32-S3's native USB
Serial/JTAG), at **115200 baud**. Plug the pedal into your computer with a USB-C
data cable and use one of the scripts below.

## Why not Docker?

You build plugins in Docker, so it's natural to ask for a Docker monitor too —
but it can't work on macOS or Windows. Docker Desktop runs containers inside a
Linux VM and **does not pass host USB serial devices through** to it, so a
container simply can't see the port. (Only native Linux Docker can, via
`--device`.) Monitoring therefore runs directly on the host — and the scripts
below need **nothing installed**: no ESP-IDF, no Python, no PuTTY.

## macOS / Linux

```sh
./monitor.sh            # auto-detect the Q-Tune and stream its console
./monitor.sh --list     # show detected serial ports (with vendor IDs) and exit
./monitor.sh /dev/cu.usbmodem1101   # or name a port explicitly
```

Uses built-in `stty` + `cat`. Quit with **Ctrl-C**. Auto-detect identifies the
pedal by its USB **vendor ID** (Espressif `0x303A`), so it picks the right port
even with other serial devices (Bluetooth, USB-UART adapters) connected, and
regardless of which USB-C port / `usbmodem` number it lands on.

## Windows

```powershell
.\monitor.ps1           # auto-detect the COM port
.\monitor.ps1 -List     # show detected serial ports (with vendor IDs) and exit
.\monitor.ps1 COM7      # or name it explicitly
```

Uses the built-in .NET `SerialPort` class (no PuTTY needed). Quit with
**Ctrl-C**. Auto-detect matches the pedal's USB **vendor ID** (Espressif
`0x303A`). In Device Manager the pedal shows up under *Ports (COM & LPT)* as
**USB JTAG/serial debug unit** or **USB Serial Device**.

> If PowerShell blocks the script, allow it for this session with:
> `powershell -ExecutionPolicy Bypass -File .\monitor.ps1`

## Seeing boot output

Most load failures are logged during startup. The monitor **reconnects
automatically**, so you don't have to time anything:

1. Start the monitor (`./monitor.sh` or `.\monitor.ps1`) and leave it running.
2. **Power-cycle the pedal** (unplug/replug it, or use the restart option). The
   USB connection drops and the monitor prints `[disconnected …]`.
3. When the pedal comes back it prints `[connected: …]` and the boot log scrolls
   by — re-detecting the port by vendor ID, so a changed `usbmodem` / COM number
   doesn't matter. Plugin scan results look like:
   ```
   I (1234) PLUGINS: loaded my_tuner.so -> slot 101 (qtune.my-tuner.k7f2q9)
   W (1240) PLUGINS: rejected glow.so: missing symbol qtune_plugin_entry
   ```

You can start the monitor *before* the pedal is even plugged in — it waits for it
to appear. Quit with **Ctrl-C**.

(The scripts deliberately don't toggle the DTR/RTS lines, which on the ESP32-S3
control reset/boot-mode — so a manual power-cycle is the way to restart.)

> Tiny caveat: the first few ROM-bootloader bytes right at power-on can be missed
> in the moment the USB re-enumerates. The application log (`ESP_LOG` / your
> `printf`s), which is what matters for plugin debugging, comes a beat later and
> is captured in full.

## Decoding a crash backtrace

A Guru Meditation crash prints a backtrace of raw addresses:

```
Backtrace: 0x420ffe56:0x3fccaa30 0x42052339:0x3fccaa50 ...
```

The raw text already tells you a plugin crashed and roughly where. To turn those
addresses into **file:line**, you need the full `esp-idf-monitor` (it has the ELF
and the toolchain to symbolize them) — that's part of an ESP-IDF install and is
beyond these zero-install scripts. For most plugin debugging the plain log
(the error message and which plugin) is enough; see
[TROUBLESHOOTING.md](TROUBLESHOOTING.md).
