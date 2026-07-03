param(
  [string] $SourceDir = ".\ghostesp-flipper-installer",
  [string] $ZipFile = ".\ghostesp-flipper-installer.zip"
)
if (-not (Test-Path $SourceDir)) { Write-Error "Nie znaleziono $SourceDir"; exit 1 }
if (Test-Path $ZipFile) { Remove-Item $ZipFile -Force }
Compress-Archive -Path (Join-Path $SourceDir '*') -DestinationPath $ZipFile -Force
Write-Host "Utworzono: $ZipFile"
exit 0
