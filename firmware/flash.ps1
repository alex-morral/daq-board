# Flash the DAQ Board firmware via ST-Link + OpenOCD.
# Builds first, then programs. Requires the ST-Link connected to J3 (SWD).

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# Build first
& (Join-Path $root "build.ps1")

# Locate openocd (xPack install under C:\openocd, or on PATH)
$ocdExe = (Get-Command openocd -ErrorAction SilentlyContinue).Source
if (-not $ocdExe) {
    $dir = Get-ChildItem "C:\openocd" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($dir) { $ocdExe = Join-Path $dir.FullName "bin\openocd.exe" }
}
if (-not $ocdExe -or -not (Test-Path $ocdExe)) { throw "openocd not found" }

$elf = Join-Path $root "build\daq.elf"
& $ocdExe -f "interface/stlink.cfg" -f "target/stm32f1x.cfg" `
    -c "program `"$elf`" verify reset exit"
if ($LASTEXITCODE -ne 0) { throw "Flash failed" }
Write-Host "Flashed OK" -ForegroundColor Green
