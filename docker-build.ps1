<#
.SYNOPSIS
    Build a Q-Tune plugin inside the pinned ESP-IDF v5.3.2 container on Windows.

.DESCRIPTION
    The PowerShell twin of docker-build.sh. Requires only Docker Desktop — no
    local ESP-IDF, Python, or pyelftools. The SDK and project are mounted at
    fixed container paths (/qtune-sdk and /project) and QTUNE_SDK_DIR is
    overridden for the build, so the build never depends on the host path layout
    (Windows drive letters and backslashes are fine as Docker mount *sources*).

    After a successful build the plugin is validated automatically (the container
    already ships python3 + pyelftools). The resulting <name>.so is written to
    <plugin-project-dir>\build\.

.EXAMPLE
    .\docker-build.ps1 examples\example_tuner

.EXAMPLE
    .\docker-build.ps1            # builds the project in the current directory
#>

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy

$ErrorActionPreference = 'Stop'

$Image   = 'espressif/idf:v5.3.2'
$SdkRoot = (Resolve-Path -LiteralPath $PSScriptRoot).ProviderPath

$ProjectArg = if ($args.Count -ge 1) { $args[0] } else { '.' }
if (-not (Test-Path -LiteralPath (Join-Path $ProjectArg 'CMakeLists.txt'))) {
    Write-Error "no CMakeLists.txt in '$ProjectArg' — point this at a plugin project dir."
    exit 1
}
$ProjectDir = (Resolve-Path -LiteralPath $ProjectArg).ProviderPath

# Fail early with a friendly message if Docker isn't installed or not running.
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error "Docker is not installed. Install Docker Desktop: https://docs.docker.com/get-docker/"
    exit 1
}
docker info *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Error "Docker is installed but not running. Start Docker Desktop and try again."
    exit 1
}

Write-Host "Building plugin in: $ProjectDir"
Write-Host "Mounting SDK:       $SdkRoot  ->  /qtune-sdk (read-only)"
Write-Host "Using image:        $Image"

# Same in-container command as docker-build.sh: skip set-target when the project
# is already configured for esp32s3 (keeps builds incremental), build with the
# overridden QTUNE_SDK_DIR, then run the offline validator.
$Cmd = @'
set -e
DEF=-DQTUNE_SDK_DIR=/qtune-sdk
grep -q "CONFIG_IDF_TARGET=\"esp32s3\"" sdkconfig 2>/dev/null || idf.py $DEF set-target esp32s3
idf.py $DEF build
cmake --build build --target validate
'@

docker run --rm -t `
    -v "${SdkRoot}:/qtune-sdk:ro" `
    -v "${ProjectDir}:/project" `
    -w /project `
    -e IDF_TARGET=esp32s3 `
    $Image `
    sh -c $Cmd

exit $LASTEXITCODE
