<#
.SYNOPSIS
    Watch the Q-Tune's serial console on Windows. Zero install.

.DESCRIPTION
    The pedal logs boot and runtime messages over its USB-C port (the ESP32-S3
    native USB Serial/JTAG, 115200 baud). This streams that output using the
    built-in .NET SerialPort class — no ESP-IDF, Python, PuTTY, or Docker.

    It identifies the pedal by USB vendor ID (Espressif = 0x303A) so it picks the
    right COM port even when other serial devices are present, and it RECONNECTS
    automatically: leave it running, power-cycle or replug the pedal, and it waits
    for the device to come back and resumes — so you can watch the boot logs.

    Why not Docker? Docker Desktop can't pass a USB serial / COM device into its
    Linux VM, so a container can't see the port. See docs/MONITOR.md.

.PARAMETER Port
    COM port to open (e.g. COM7). Auto-detected if omitted.

.PARAMETER List
    List detected serial ports and exit.

.EXAMPLE
    .\monitor.ps1
    .\monitor.ps1 -List
    .\monitor.ps1 COM7

.NOTES
    Quit with Ctrl-C.
#>

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy

param(
    [string]$Port = "",
    [switch]$List
)

$ErrorActionPreference = 'Stop'
$baud = 115200

# Espressif (native USB Serial/JTAG) first, then common USB-UART bridges.
$EspressifVid = 0x303A
$BridgeVids   = @(0x10C4, 0x1A86, 0x0403, 0x2341)  # CP210x, CH340, FTDI, Arduino

function Get-QtPorts {
    # Each PnP serial entity carries its COM name in the Caption and VID/PID in
    # the PNPDeviceID (e.g. "USB\VID_303A&PID_1001\..."). Bluetooth COM ports have
    # no USB VID, so this naturally favors real USB serial devices.
    Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Caption -match '\(COM\d+\)' } |
        ForEach-Object {
            $com = [regex]::Match($_.Caption, 'COM\d+').Value
            $vid = 0; $prodId = 0
            if ($_.PNPDeviceID -match 'VID_([0-9A-Fa-f]{4})') { $vid    = [Convert]::ToInt32($Matches[1], 16) }
            if ($_.PNPDeviceID -match 'PID_([0-9A-Fa-f]{4})') { $prodId = [Convert]::ToInt32($Matches[1], 16) }
            [PSCustomObject]@{ Port = $com; Vid = $vid; Pid = $prodId; Name = $_.Caption }
        }
}

function Resolve-QtPort {
    $ports = @(Get-QtPorts)
    if ($ports.Count -eq 0) { return $null }
    $sel = $ports | Where-Object { $_.Vid -eq $EspressifVid } | Select-Object -First 1
    if (-not $sel) { $sel = $ports | Where-Object { $BridgeVids -contains $_.Vid } | Select-Object -First 1 }
    if (-not $sel -and $ports.Count -eq 1) { $sel = $ports[0] }
    if ($sel) { return $sel.Port }
    return $null
}

if ($List) {
    Write-Host "Detected serial ports:"
    $ports = @(Get-QtPorts)
    if ($ports.Count -eq 0) { Write-Host "  (no USB serial COM ports found)" }
    foreach ($p in $ports) {
        Write-Host ("  {0,-6} vid=0x{1:X4} pid=0x{2:X4}  {3}" -f $p.Port, $p.Vid, $p.Pid, $p.Name)
    }
    exit 0
}

Write-Host "Q-Tune serial monitor @ $baud baud - Ctrl-C to quit."
Write-Host "Power-cycle or replug the pedal anytime; it reconnects and shows boot logs."
Write-Host "------------------------------------------------------------"

# Reconnect loop: stream while the device is present; when it disconnects (or
# before it first appears), wait and re-detect. Re-detecting (when no explicit
# -Port was given) handles the COM number changing across a re-enumeration.
$announcedWait = $false
while ($true) {
    $com = if ($Port) { $Port } else { Resolve-QtPort }
    if (-not $com) {
        if (-not $announcedWait) {
            Write-Host "[waiting for the Q-Tune - connect it or power-cycle ...]"
            $announcedWait = $true
        }
        Start-Sleep -Milliseconds 300
        continue
    }
    $announcedWait = $false

    $sp = New-Object System.IO.Ports.SerialPort($com, $baud,
        [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    # Don't toggle DTR/RTS — on the ESP32-S3 those lines drive reset/boot select,
    # and asserting them can drop the chip into the download (bootloader) stub.
    $sp.DtrEnable = $false
    $sp.RtsEnable = $false
    $sp.ReadTimeout = 500
    try {
        $sp.Open()
        Write-Host "[connected: $com]"
        while ($true) {
            try { Write-Host $sp.ReadLine() }
            catch [System.TimeoutException] { }
        }
    }
    # Catch ONLY disconnect / open-failure exceptions — not Ctrl-C, so quitting
    # still works. IOException = device removed; InvalidOperation = port closed;
    # UnauthorizedAccess = port busy / still enumerating.
    catch [System.IO.IOException], [System.InvalidOperationException], [System.UnauthorizedAccessException] {
        Write-Host "[disconnected - waiting for the pedal to come back ...]"
    }
    finally {
        if ($sp.IsOpen) { try { $sp.Close() } catch { } }
        $sp.Dispose()
    }
    Start-Sleep -Milliseconds 300
}
