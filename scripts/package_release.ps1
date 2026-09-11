param(
    [string]$Version = "0.2.2"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$OutRoot = Join-Path $Root "releases\v$Version\src"
$TarName = "edgehub-client-sdk-$Version-src.tar.gz"

New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null

$copyDirs = @(
    @{ Src = "include"; Dst = "include" },
    @{ Src = "src"; Dst = "src" },
    @{ Src = "port"; Dst = "port" },
    @{ Src = "user_template"; Dst = "user_template" },
    @{ Src = "docs"; Dst = "docs" }
)
foreach ($d in $copyDirs) {
    $src = Join-Path $Root $d.Src
    $dst = Join-Path $OutRoot $d.Dst
    if (Test-Path $src) {
        New-Item -ItemType Directory -Force -Path $dst | Out-Null
        Copy-Item (Join-Path $src "*") $dst -Recurse -Force
    }
}

foreach ($name in @("CMakeLists.txt", "CHANGELOG.md", "LICENSE", "README.md", "VERSION")) {
    $src = Join-Path $Root $name
    if (Test-Path $src) {
        Copy-Item $src $OutRoot -Force
    }
}

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
    tar -czf $TarName -C src .
    Pop-Location
    Write-Host "Release: $(Join-Path $Root "releases\v$Version\$TarName")"
}

Write-Host "Package ready: $OutRoot"
