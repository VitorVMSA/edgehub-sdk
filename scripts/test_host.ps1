param(
  [switch]$DeviceSdk
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$argsC = @("-S", ".", "-B", "build", "-DEHC_BUILD_TESTS=ON", "-DEHC_TEST_DEVICE_SDK=ON")
if ($DeviceSdk) { Write-Host "EHC_TEST_DEVICE_SDK=ON (default in-tree)" }
cmake @argsC
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
