$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build\host"
$DistDir = Join-Path $Root "dist\host"

New-Item -ItemType Directory -Force -Path $BuildDir, $DistDir | Out-Null

cmake -S $Root -B $BuildDir -DEH_BUILD_HOST=ON
cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "Host library build failed" }

$LibName = "libedgehub_device.a"
if ($IsWindows -or $env:OS -match "Windows") {
    $LibPath = Join-Path $BuildDir "Release\edgehub_device.lib"
    if (-not (Test-Path $LibPath)) {
        $LibPath = Join-Path $BuildDir "libedgehub_device.a"
    }
} else {
    $LibPath = Join-Path $BuildDir "libedgehub_device.a"
}

if (-not (Test-Path $LibPath)) {
    throw "Static library not found after build: $LibPath"
}

Copy-Item $LibPath (Join-Path $DistDir "libedgehub_device.a") -Force
Copy-Item (Join-Path $Root "include\edgehub_device.h") (Join-Path $DistDir "edgehub_device.h") -Force
Write-Host "Host build OK: $DistDir"

# Run simulator test
$SimBuild = Join-Path $Root "build\host_simulator"
cmake -S (Join-Path $Root "examples\host_simulator") -B $SimBuild
cmake --build $SimBuild --config Release
$SimExe = Join-Path $SimBuild "Release\host_simulator.exe"
if (-not (Test-Path $SimExe)) {
    $SimExe = Join-Path $SimBuild "host_simulator.exe"
}
if (-not (Test-Path $SimExe)) {
    $SimExe = Join-Path $SimBuild "host_simulator"
}
& $SimExe
if ($LASTEXITCODE -ne 0) { throw "host_simulator failed" }
