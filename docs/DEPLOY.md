# Deploying Your Plugin to Q-Tune

Once you have a built `.so` file, this guide shows how to get it onto your Q-Tune pedal, activate it, and manage it over time.

## Prerequisites

- A built plugin `.so` file (e.g., `build/my_plugin.so`).
- Your Q-Tune pedal connected to power.

## Two ways to upload: USB or Wi-Fi

Choose one. **USB Drive Mode is the recommended path** — it's quick, needs no
network setup, and works even if the pedal isn't on Wi-Fi. Wi-Fi upload is a handy
alternative once the pedal is on your network (and it's what the web tools for
deleting and re-enabling plugins use).

## Method 1: USB Drive Mode (recommended)

### 1. Connect the pedal's storage as a USB drive

1. Disconnect the pedal from power.
2. Plug it into your computer via USB.
3. **Hold the foot switch** (the large button on top).
4. While holding the foot switch, power the pedal on.
5. Keep holding for ~2 seconds after it powers on, then release.

The pedal will enter USB Drive Mode. Your computer will mount its `/data` partition as a removable drive (like you'd see a USB stick).

### 2. Drag your .so file into the plugins folder

1. On the mounted drive, you'll see a `/plugins` folder.
2. Drag your `.so` file into that folder.
3. Wait for the copy to complete.
4. Eject the drive safely (macOS: drag to Trash, Windows: "Eject", Linux: `umount`).

The file is now at `/data/plugins/my_plugin.so` on the pedal.

## Method 2: Wi-Fi upload

### 1. Connect your pedal to Wi-Fi

1. On the pedal, navigate to **Settings → Wi-Fi**.
2. Select your network and enter the password.
3. Wait for a connection. The pedal's IP address will be shown on the screen (e.g., `192.168.1.100`).

### 2. Open the plugin upload page

In your browser (desktop, tablet, or phone), navigate to:

```
http://<device-ip>/plugins
```

Replace `<device-ip>` with the IP address shown in step 1. For example:

```
http://192.168.1.100/plugins
```

You should see a page with:
- A list of installed plugins (if any).
- An upload form at the top.

### 3. Upload your .so file

1. Click the upload form's file picker.
2. Select your `.so` file (e.g., `build/my_plugin.so`).
3. Click **Upload**.

The file is sent to the pedal and stored at `/data/plugins/my_plugin.so` on the pedal's internal flash.

## Step 3: Restart the pedal

After uploading (via USB or Wi-Fi), the plugin is not yet active. You must restart the firmware.

**Option A: Power-cycle**
1. Power off the pedal.
2. Wait 2 seconds.
3. Power on.

**Option B: Software restart (Wi-Fi)**
1. Go to **Settings → Restart** on the pedal.
2. Confirm.

**Option C: /plugins web page**
If you're still at `http://<device-ip>/plugins` in your browser, you may see a **Restart** button. Click it to restart over Wi-Fi without touching the pedal.

At boot, the firmware scans `/data/plugins/`, loads each `.so` via the ELF loader, and registers all valid plugins. Invalid or crash-disabled plugins are skipped.

## Step 4: Activate your plugin

After restart, your plugin appears in the appropriate settings menu:

- **Tuner plugins**: Go to **Settings → Tuner → Style** and select your plugin by name.
- **Standby plugins**: Go to **Settings → Display → Standby Screen** and select your plugin by name.

The selection is saved to the pedal's non-volatile storage. Next time you power on, that plugin will be active.

## Managing installed plugins

Plugins live as `.so` files in the pedal's `/data/plugins/` folder (it shows up as
`/plugins` on the USB drive). You can manage that folder **either way**, whichever
is handier — the rest of this guide just refers to the `/plugins` folder:

- **USB Drive Mode** — mount the pedal (hold the foot switch at power-on) and add,
  replace, delete, or rename files in `/plugins` directly.
- **The `/plugins` web page** — over Wi-Fi at `http://<device-ip>/plugins`; it also
  shows each plugin's type, ID, and enabled/disabled status, with Upload, Delete,
  and Re-enable buttons.

Any change takes effect after a restart.

## Updating your plugin

To update an existing plugin:

1. Build a new version and get a fresh `.so` file.
2. Copy it over (USB Drive Mode or Wi-Fi) using the same **filename**. The new file overwrites the old one.
3. Restart the pedal.

Your updated plugin will load and replace the old one.

## Replacing a plugin with a different one

If you want to switch to a completely different plugin:

1. **Option A**: Copy the new `.so` over (USB Drive Mode or Wi-Fi), restart, and select it in the menu.
2. **Option B**: Go to **Settings** and switch to a different plugin without deleting the old one (both can coexist).

Multiple plugins can be installed at once. Only the one selected in the menu is active.

## Deleting a plugin

Remove `<name>.so` from the `/plugins` folder — delete the file over USB Drive
Mode, or use the **Delete** button on the `/plugins` web page. Restart for it to
leave the menu.

## Viewing installed plugins

List the `/plugins` folder to see which `.so` files are installed. The `/plugins`
web page shows more — each plugin's name, type (Tuner or Standby), ID, and
enabled/disabled status (crash-quarantined plugins show as disabled), plus
Upload/Delete/Re-enable buttons.

## Crash-disabled plugins

If a plugin crashes during init or display, the firmware quarantines it (see `docs/TROUBLESHOOTING.md`). The filename becomes `<name>.so.disabled`. The plugin won't load on restart.

To re-enable a crash-disabled plugin, rename its file from `<name>.so.disabled`
back to `<name>.so` in the `/plugins` folder (over USB Drive Mode), or click
**Re-enable** on the `/plugins` web page. It loads again on the next restart. (If
it crashes again, it's disabled again after 2 strikes.)

If your plugin keeps crashing, see `docs/TROUBLESHOOTING.md` for common causes and fixes.

## Troubleshooting deployment

| Problem | Likely cause | Fix |
|---------|--------------|-----|
| Browser can't reach `http://<ip>/plugins` | Pedal not connected to Wi-Fi, or wrong IP | Go to Settings → Wi-Fi and verify connection; check the displayed IP |
| Upload succeeds but plugin doesn't appear in menu after restart | Missing/invalid `uid`, or two plugins sharing the same `uid` | See `docs/TROUBLESHOOTING.md`: "Plugin doesn't appear in the menu" |
| Pedal crashes when I select the plugin | Plugin has a bug (timer not deleted, etc.) | See `docs/TROUBLESHOOTING.md` for crash recovery |
| USB Drive Mode won't mount | Didn't hold foot switch, or timing issue | Try again: foot switch first, then power, hold for 2+ seconds |
| Can't delete a file via USB | Drive isn't unmounted/ejected properly | Eject the drive in your OS, unplug, and try again |

## Recovery

If a plugin breaks the pedal entirely:

1. See `docs/TROUBLESHOOTING.md` for Safe Mode recovery.
2. Hold **BOOT button (GPIO0)** at power-on to skip all plugins and run only built-ins.
3. Once in Safe Mode, remove the bad plugin from the `/plugins` folder — over USB
   Drive Mode, or from the `/plugins` web page over Wi-Fi.

## Quick checklist

- [ ] Plugin `.so` file built and ready.
- [ ] Pedal in USB Drive Mode (Method 1) or connected to Wi-Fi (Method 2).
- [ ] `.so` uploaded to `/data/plugins/`.
- [ ] Pedal restarted (power-cycle, Settings → Restart, or `/plugins` Restart button).
- [ ] Plugin appears in Settings → Tuner → Style (or Standby Screen).
- [ ] Plugin selected and active.
- [ ] Works as expected.
