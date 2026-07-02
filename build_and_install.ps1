param(
  [string] $MountPath
)

# Kombinacja: buduje .fap i kopiuje ją do Flippera (automatyczne wykrycie mount lub -MountPath)
# Usage:
#   .\build_and_install.ps1
#   .\build_and_install.ps1 -MountPath "E:\"

# 1) Build
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$buildScript = Join-Path $scriptDir 'build_fap.ps1'
if (-not (Test-Path $buildScript)) {
  Write-Error "Nie znaleziono build_fap.ps1 w tym samym katalogu. Umieść oba pliki razem."; exit 1
}

Write-Host "Buduję ghostesp.fap..."
& $buildScript -CopyTo $env:TEMP
if ($LASTEXITCODE -ne 0) { Write-Error "Build nieudany"; exit 2 }

$built = Join-Path $env:TEMP 'ghostesp.fap'
if (-not (Test-Path $built)) {
  # build_fap może zapisać plik w repo\dist
  # spróbuj znaleźć dist\ghostesp.fap obok build_fap.ps1 (repoRoot)
  $repoRoot = $env:GHOSTESP_DIR
  if (-not $repoRoot) { $repoRoot = (Get-Location).Path }
  $candidate = Join-Path $repoRoot 'dist\ghostesp.fap'
  if (Test-Path $candidate) { $built = $candidate } else { Write-Error "Nie znaleziono zbudowanego ghostesp.fap"; exit 3 }
}

# 2) Install (kopiowanie)
$installScript = Join-Path $scriptDir 'install_flipper.ps1'
if (-not (Test-Path $installScript)) { Write-Error "Nie znaleziono install_flipper.ps1"; exit 4 }

Write-Host "Kopiuję do Flippera..."
& $installScript -MountPath $MountPath -LocalFile $built
if ($LASTEXITCODE -ne 0) { Write-Error "Kopiowanie nieudane"; exit 5 }

Write-Host "Gotowe."
exit 0
