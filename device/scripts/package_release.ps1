param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("host", "esp32s3")]
    [string]$Target,
    [string]$Version = "1.2.4"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$OutRoot = Join-Path $Root "releases\v$Version\$Target"
$TarName = "edgehub-device-sdk-$Version-$Target.tar.gz"

New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null

$DistDir = Join-Path $Root "dist\$Target"
if (-not (Test-Path (Join-Path $DistDir "libedgehub_device.a"))) {
    if ($Target -eq "host") {
        & (Join-Path $PSScriptRoot "build_host.ps1")
    } else {
        & (Join-Path $PSScriptRoot "build_esp32s3.ps1")
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $OutRoot "include"), (Join-Path $OutRoot "lib"), (Join-Path $OutRoot "cmake") | Out-Null
Copy-Item (Join-Path $DistDir "libedgehub_device.a") (Join-Path $OutRoot "lib\") -Force
Copy-Item (Join-Path $Root "include\edgehub_device.h") (Join-Path $OutRoot "include\") -Force
Copy-Item (Join-Path $Root "cmake\edgehub_device.cmake") (Join-Path $OutRoot "cmake\") -Force
Copy-Item (Join-Path $Root "CHANGELOG.md") $OutRoot -Force
Copy-Item (Join-Path $Root "LICENSE") $OutRoot -Force -ErrorAction SilentlyContinue
$NotesDir = Join-Path $Root "releases\v$Version"
$notesTag = $Version -replace '\.', ''
foreach ($name in @("release_notes_$notesTag.txt", "SYNC.txt", "MANIFEST.txt", "VERSION")) {
    $src = Join-Path $NotesDir $name
    if (Test-Path $src) {
        Copy-Item $src $OutRoot -Force
    }
}

if (Get-Command tar -ErrorAction SilentlyContinue) {
    Push-Location (Join-Path $Root "releases\v$Version")
    tar -czf $TarName -C $Target .
    Pop-Location
    Write-Host "Release: $(Join-Path $Root "releases\v$Version\$TarName")"
}

Write-Host "Package ready: $OutRoot"
