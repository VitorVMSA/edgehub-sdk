param(
  [string]$Source = "",
  [string]$Dest = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $Dest) { $Dest = Join-Path $root "third_party\edgehub-device-sdk" }
if (-not $Source) { $Source = Join-Path (Split-Path -Parent $root) "edgehub-device-sdk" }

if (-not (Test-Path (Join-Path $Source "CMakeLists.txt"))) {
  Write-Error "device SDK not found at $Source (expected tag >= v1.2.3)"
}

New-Item -ItemType Directory -Force -Path (Split-Path $Dest) | Out-Null
if (Test-Path $Dest) {
  Write-Host "already present: $Dest"
  exit 0
}

try {
  New-Item -ItemType Junction -Path $Dest -Target $Source | Out-Null
  Write-Host "junction $Dest -> $Source"
} catch {
  Copy-Item -Recurse $Source $Dest
  Write-Host "copied $Source -> $Dest"
}
