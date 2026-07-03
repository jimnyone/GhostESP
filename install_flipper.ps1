param(
  [string] $MountPath,
  [string] $LocalFile
)

# Install GhostESP .fap to Flipper (copy to <FLIPPER>\apps\Tools\ghostesp.fap)
# Usage:
#   .\install_flipper.ps1                     # wykryje Flipper i spróbuje pobrać z Releases
#   .\install_flipper.ps1 -LocalFile "C:\path\ghostesp.fap"
#   .\install_flipper.ps1 -MountPath "E:\"

$repo = "jimnyone/GhostESP"
$fap = "ghostesp.fap"
$download = "https://github.com/$repo/releases/latest/download/$fap"

function Find-Flipper {
  param([string] $cand)
  if ($cand) {
    $candPath = $cand.TrimEnd('\')
    if (Test-Path (Join-Path $candPath "apps")) { return $candPath }
  }
  foreach ($d in (Get-PSDrive -PSProvider FileSystem)) {
    $root = $d.Root.TrimEnd('\')
    if (Test-Path (Join-Path $root "apps")) { return $root }
  }
  return $null
}

Write-Host "GhostESP Flipper installer (Windows)"

if ($LocalFile) {
  if (-not (Test-Path $LocalFile)) {
    Write-Error "Lokalny plik nie istnieje: $LocalFile"; exit 2
  }
}

$mount = Find-Flipper -cand $MountPath
if (-not $mount) {
  Write-Host "Nie wykryto Flippera automatycznie."
  $input = Read-Host "Podaj literę dysku / punkt montowania Flippera (np. E:\) lub naciśnij Enter aby wyszukać ponownie"
  if ($input) { $mount = Find-Flipper -cand $input }
}

if (-not $mount) {
  Write-Host "Nie znaleziono punktu montowania Flippera. Możesz ręcznie skopiować $fap do <FLIPPER>/apps/Tools/"; exit 3
}

Write-Host "Flipper znaleziono pod: $mount"

$temp = Join-Path $env:TEMP ("ghostesp_" + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $temp -Force | Out-Null
$out = Join-Path $temp $fap

try {
  if ($LocalFile) {
    Copy-Item -Path $LocalFile -Destination $out -Force
    Write-Host "Używam lokalnego pliku: $LocalFile"
  } else {
    Write-Host "Pobieram $fap z $download ..."
    Invoke-WebRequest -Uri $download -OutFile $out -UseBasicParsing -ErrorAction Stop
    Write-Host "Pobrano $fap"
  }

  $dest = Join-Path $mount "apps\Tools"
  New-Item -ItemType Directory -Path $dest -Force | Out-Null
  Copy-Item -Path $out -Destination (Join-Path $dest $fap) -Force
  Write-Host "Zainstalowano: $dest\$fap"
  Write-Host "Bezpiecznie odłącz Flippera."
} catch {
  Write-Error "Wystąpił błąd: $_"
  Write-Host "Jeśli pobieranie z Releases się nie powiodło, pobierz ghostesp.fap ręcznie i użyj -LocalFile."
  exit 4
} finally {
  Remove-Item -Recurse -Force $temp -ErrorAction SilentlyContinue
}
exit 0
