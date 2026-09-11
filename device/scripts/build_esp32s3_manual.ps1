# Build libedgehub_device.a for ESP32-S3 using Xtensa toolchain (no full idf.py export).
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$OutDir = Join-Path $Root "dist\esp32s3"
$WorkDir = Join-Path $Root "build\esp32s3-manual"
$ObjDir = Join-Path $WorkDir "obj"

$ToolBase = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin"
if (-not (Test-Path $ToolBase)) {
    throw "Xtensa toolchain not found at $ToolBase"
}

$CC = Join-Path $ToolBase "xtensa-esp32s3-elf-gcc.exe"
$AR = Join-Path $ToolBase "xtensa-esp32s3-elf-ar.exe"

$Sources = @(
    "$Root/src/edgehub_device.c",
    "$Root/src/internal/eh_auth.c",
    "$Root/src/internal/eh_b64.c",
    "$Root/src/internal/eh_checkin.c",
    "$Root/src/internal/eh_command_executed.c",
    "$Root/src/internal/eh_commands.c",
    "$Root/src/internal/eh_hmac.c",
    "$Root/src/internal/eh_identity.c",
    "$Root/src/internal/eh_json.c",
    "$Root/src/internal/eh_log.c",
    "$Root/src/internal/eh_provision.c",
    "$Root/src/internal/eh_sha256.c",
    "$Root/src/internal/eh_telemetry.c",
    "$Root/src/internal/eh_telemetry_queue.c",
    "$Root/src/internal/ed25519_donna/ed25519.c"
)

New-Item -ItemType Directory -Force -Path $OutDir, $ObjDir | Out-Null

$Includes = @(
    "-I$Root/include",
    "-I$Root/src/internal/ed25519_donna"
)

$Flags = @(
    "-mlongcalls",
    "-fno-builtin-memcpy",
    "-fno-builtin-memset",
    "-fno-builtin-bzero",
    "-ffunction-sections",
    "-fdata-sections",
    "-Os",
    "-Wno-implicit-fallthrough",
    "-DED25519_REFHASH",
    "-DED25519_CUSTOMRANDOM"
) + $Includes

$Objects = @()
foreach ($src in $Sources) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $ObjDir "$base.o"
    & $CC @Flags -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $src" }
    $Objects += $obj
}

$LibPath = Join-Path $OutDir "libedgehub_device.a"
if (Test-Path $LibPath) { Remove-Item $LibPath -Force }
& $AR rcs $LibPath @Objects
if ($LASTEXITCODE -ne 0) { throw "ar failed" }

Copy-Item (Join-Path $Root "include\edgehub_device.h") (Join-Path $OutDir "edgehub_device.h") -Force
Write-Host "ESP32-S3 manual build OK: $LibPath"
