param(
  [string] $CopyTo,
  [string] $OutputFile,
  [switch] $NoDeps
)

# Buduje ghostesp.fap z kodu źródłowego repo. Uruchom w katalogu repo lub ustaw env GHOSTESP_DIR.
# Usage:
#   .\build_fap.ps1
#   .\build_fap.ps1 -CopyTo "E:\apps\Tools"
#   .\build_fap.ps1 -OutputFile "C:\temp\ghostesp.fap"

function Fail($msg) { Write-Error $msg; exit 1 }

$repoRoot = $env:GHOSTESP_DIR
if (-not $repoRoot) { $repoRoot = (Get-Location).Path }

if (-not (Test-Path (Join-Path $repoRoot "build.py"))) {
  Fail "Nie znaleziono build.py w katalogu $repoRoot. Uruchom skrypt z katalogu repo lub ustaw GHOSTESP_DIR."
}

# Find python
$python = (Get-Command python3 -ErrorAction SilentlyContinue).Source
if (-not $python) { $python = (Get-Command python -ErrorAction SilentlyContinue).Source }
if (-not $python) { Fail "Nie znaleziono Pythona w PATH (wymagany >=3.7)." }

$ver = & $python -c "import sys; print('{}.{}'.format(sys.version_info.major, sys.version_info.minor))"
$parts = $ver.Split('.')
if ([int]$parts[0] -lt 3 -or ([int]$parts[0] -eq 3 -and [int]$parts[1] -lt 7)) {
  Fail "Wymagany Python >= 3.7. Znaleziono: $ver"
}
Write-Host "Używam Pythona: $python (ver $ver)"

# Check required toolchain
$reqs = @('arm-none-eabi-gcc','cmake','make')
$missing = @()
foreach ($r in $reqs) {
  if (-not (Get-Command $r -ErrorAction SilentlyContinue)) {
    $missing += $r
  }
}
if ($missing.Count -gt 0) {
  Fail "Brakuje narzędzi: $($missing -join ', '). Zainstaluj je (np. przez MSYS2, Chocolatey, lub oficjalny toolchain)."
}

# Install python deps
if (-not $NoDeps) {
  $reqFile = Join-Path $repoRoot 'requirements.txt'
  if (Test-Path $reqFile) {
    Write-Host "Instaluję zależności Python (user) z $reqFile ..."
    & $python -m pip install --user -r $reqFile
  } else {
    Write-Host "Nie znaleziono requirements.txt — pomijam instalację zależności."
  }
} else {
  Write-Host "-NoDeps: pomijam instalację zależności"
}

Push-Location $repoRoot
Write-Host "Uruchamiam: $python build.py flipper"
& $python build.py flipper
Pop-Location

$distFile = Join-Path $repoRoot 'dist\ghostesp.fap'
if (-not (Test-Path $distFile)) {
  Fail "Build zakończony, ale nie znaleziono $distFile"
}
Write-Host "Znaleziono: $distFile"

if ($OutputFile) {
  $destDir = Split-Path $OutputFile -Parent
  if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
  Copy-Item -Path $distFile -Destination $OutputFile -Force
  Write-Host "Skopiowano do: $OutputFile"
}

if ($CopyTo) {
  if (-not (Test-Path $CopyTo)) { New-Item -ItemType Directory -Path $CopyTo -Force | Out-Null }
  Copy-Item -Path $distFile -Destination $CopyTo -Force
  Write-Host "Skopiowano do: $CopyTo"
}

Write-Host "Build .fap zakończony pomyślnie."
Write-Host "Plik: $distFile"
exit 0
