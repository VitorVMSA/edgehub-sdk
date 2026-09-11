$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ExampleDir = Join-Path $Root "examples\esp32_minimal"
$BuildDir = Join-Path $ExampleDir "build"
$DistDir = Join-Path $Root "dist\esp32s3"
$ManualScript = Join-Path $Root "scripts\build_esp32s3_manual.ps1"

function Copy-DistFromLib($LibPath) {
    New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
    Copy-Item $LibPath (Join-Path $DistDir "libedgehub_device.a") -Force
    Copy-Item (Join-Path $Root "include\edgehub_device.h") (Join-Path $DistDir "edgehub_device.h") -Force
    Write-Host "ESP32-S3 build OK: $DistDir"
}

if (-not $env:IDF_PATH) {
    foreach ($c in @("C:\esp\v5.5.4\esp-idf", "C:\Espressif\frameworks\esp-idf-v5.5.4")) {
        if (Test-Path $c) { $env:IDF_PATH = $c; break }
    }
}

$IdfOk = $false
if ($env:IDF_PATH) {
    $env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v5.5.4\venv"
    $env:PATH = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\python\v5.5.4\venv\Scripts;" + $env:PATH
    $PythonExe = "C:\Espressif\tools\python\v5.5.4\venv\Scripts\python.exe"
    $idfPy = Join-Path $env:IDF_PATH "tools\idf.py"
    if ((Test-Path $PythonExe) -and (Test-Path $idfPy)) {
        Push-Location $ExampleDir
        try {
            & $PythonExe $idfPy -DIDF_TARGET=esp32s3 -B $BuildDir build
            if ($LASTEXITCODE -eq 0) {
                $found = Get-ChildItem -Path $BuildDir -Recurse -Filter "libedgehub_device.a" -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($found) {
                    Copy-DistFromLib $found.FullName
                    $IdfOk = $true
                }
            }
        } finally {
            Pop-Location
        }
    }
}

if (-not $IdfOk) {
    Write-Host "idf.py unavailable or failed — using manual Xtensa build"
    & $ManualScript
}
