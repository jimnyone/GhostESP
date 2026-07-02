# PowerShell wrapper for Windows
# Usage: .\build-windows.ps1

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Host "Python nie jest zainstalowany lub nie znajduje się w PATH. Pobierz z https://www.python.org/downloads/" -ForegroundColor Yellow
    exit 1
}

Write-Host "Uruchamiam build.py flipper..."
python tools\build.py flipper

if ($LASTEXITCODE -eq 0) {
    Write-Host "Budowanie zakończone sukcesem." -ForegroundColor Green
} else {
    Write-Host "Build zakończył się błędem (kod: $LASTEXITCODE)." -ForegroundColor Red
}
