#!/usr/bin/env python3
"""
build.py — pobiera gotowy ZIP z GhostESP-Revival i tworzy uproszczony pakiet .fap dla Flipper Zero.
Użycie:
  python build.py flipper

Skrypt działa na Windows: sprawdza dostępność modułów, pobiera asset, wypakowuje/zgeneruje manifest i spakuje .fap.
"""

import sys
import os
import hashlib
import shutil
import zipfile
from pathlib import Path

try:
    import requests
except Exception:
    print("Brak modułu 'requests'. Zainstaluj: pip install requests")
    sys.exit(1)

REPO_TAG = "v2.0"
ASSET_NAME = "esp32-generic.zip"
DOWNLOAD_URL = f"https://github.com/GhostESP-Revival/GhostESP/releases/download/{REPO_TAG}/{ASSET_NAME}"

OUT_DIR = Path("dist")
TMP_DIR = Path("build_tmp")
FAP_NAME = "GhostESP-esp32-generic.fap"


def download_asset(url: str, out_path: Path):
    print(f"Pobieram asset: {url}")
    resp = requests.get(url, stream=True)
    resp.raise_for_status()
    total = resp.headers.get('Content-Length')
    if total:
        total = int(total)
        print(f"Rozmiar (z nagłówka): {total} bajtów")
    else:
        print("Rozmiar nieznany (brak nagłówka Content-Length)")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as f:
        for chunk in resp.iter_content(chunk_size=8192):
            if chunk:
                f.write(chunk)
    print("Pobrano.")
    return out_path


def make_manifest(manifest_path: Path):
    # Prosty manifest dla pakietu .fap — można rozbudować wg potrzeb
    manifest = {
        "name": "GhostESP (ESP32 generic)",
        "version": "v2.0",
        "author": "GhostESP-Revival",
        "description": "Package containing prebuilt GhostESP binaries for esp32-generic.",
        "files": [ASSET_NAME]
    }
    import json
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False))
    print(f"Zapisano manifest: {manifest_path}")


def pack_fap(source_dir: Path, out_file: Path):
    print(f"Tworzę pakiet .fap: {out_file}")
    with zipfile.ZipFile(out_file, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(source_dir.rglob('*')):
            arcname = p.relative_to(source_dir)
            zf.write(p, arcname)
    print("Gotowe.")


def main():
    if len(sys.argv) < 2 or sys.argv[1] != 'flipper':
        print("Użycie: python build.py flipper\nPobierze esp32-generic.zip i przygotuje GhostESP-esp32-generic.fap w katalogu dist/")
        return

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    TMP_DIR.mkdir(parents=True, exist_ok=True)

    asset_path = OUT_DIR / ASSET_NAME

    # Pobierz asset
    try:
        download_asset(DOWNLOAD_URL, asset_path)
    except Exception as e:
        print("Błąd pobierania assetu:", e)
        print("Upewnij się, że masz połączenie z internetem i że URL jest dostępny.")
        sys.exit(1)

    # Sprawdź rozmiar - jeśli >100MB, ostrzeż
    size = asset_path.stat().st_size
    print(f"Pobrany plik ma rozmiar: {size} bajtów ({size/1024/1024:.2f} MB)")
    if size > 100*1024*1024:
        print("Uwaga: plik większy niż 100 MB — GitHub repo nie pozwoli na commity >100MB. Zamiast tego umieść plik jako release asset lub użyj Git LFS.")

    # Przygotuj strukturę do .fap
    package_dir = TMP_DIR / "fap_package"
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(parents=True)

    # Skopiuj asset do package_dir
    shutil.copy2(asset_path, package_dir / ASSET_NAME)

    # Stwórz manifest
    make_manifest(package_dir / 'manifest.json')

    # (Opcjonalnie) dodaj ikonkę lub pliki pomocnicze
    # Tworzymy prosty README w pakiecie
    (package_dir / 'README.txt').write_text('GhostESP prebuilt esp32-generic package\nSource: https://github.com/GhostESP-Revival/GhostESP\n')

    fap_path = OUT_DIR / FAP_NAME
    pack_fap(package_dir, fap_path)

    # Posprzątaj
    shutil.rmtree(TMP_DIR)

    print(f"Zbudowano: {fap_path}\nZawartość: {list(zipfile.ZipFile(fap_path).namelist())}")


if __name__ == '__main__':
    main()
