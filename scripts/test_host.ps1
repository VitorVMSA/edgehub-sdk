param(
  [switch]$DeviceSdk
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$argsC = @("-S", ".", "-B", "build", "-DEHC_BUILD_TESTS=ON")
if ($DeviceSdk) { $argsC += "-DEHC_TEST_DEVICE_SDK=ON" }
cmake @argsC
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
