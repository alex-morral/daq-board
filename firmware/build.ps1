# Build the DAQ Board firmware on Windows (no `make` needed).
# Discovers the xPack arm-none-eabi-gcc toolchain automatically.

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# Locate arm-none-eabi-gcc (xPack install under C:\arm-gcc, or on PATH)
$gccExe = (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue).Source
if (-not $gccExe) {
    $dir = Get-ChildItem "C:\arm-gcc" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($dir) { $gccExe = Join-Path $dir.FullName "bin\arm-none-eabi-gcc.exe" }
}
if (-not $gccExe -or -not (Test-Path $gccExe)) { throw "arm-none-eabi-gcc not found" }

$build = Join-Path $root "build"
if (-not (Test-Path $build)) { New-Item -ItemType Directory $build | Out-Null }

$sources = Get-ChildItem (Join-Path $root "src\*.c") | ForEach-Object { $_.FullName }
$elf = Join-Path $build "daq.elf"
$ld  = Join-Path $root "stm32f103c8.ld"

$cflags = @(
    "-mcpu=cortex-m3","-mthumb","-Os","-Wall","-Wextra",
    "-ffunction-sections","-fdata-sections","-nostdlib",
    "-I", (Join-Path $root "src"),
    "-T", $ld, "-Wl,--gc-sections",
    "-o", $elf
)

& $gccExe @cflags @sources
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$sizeExe = $gccExe -replace "gcc\.exe$","size.exe"
& $sizeExe $elf
Write-Host "Built: $elf" -ForegroundColor Green
