$ErrorActionPreference = "Stop"
Write-Host "Deprecated: o device SDK vive in-tree em device/ (ABI 2 / 1.2.6)."
Write-Host "Nao copie de ../edgehub-device-sdk nem para third_party/."
Write-Host "O CMake usa EHC_DEVICE_SDK_ROOT=<repo>/device por omissao."
exit 1
