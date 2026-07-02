# GhostESP — Flipper Zero packaging (PL)

Pliki zawarte w tym PR:
- `tools/build.py` — skrypt Python pobierający esp32-generic.zip z GhostESP-Revival i budujący plik .fap
- `tools/build-windows.ps1` — wrapper PowerShell dla Windows
- `flipper/manifest.json` — podstawowy manifest pakietu
- `dist/esp32-generic.url` — bezpośredni link do assetu (skrypt build.py pobiera asset automatycznie)

## Instrukcja (Windows)

### 1. Klonowanie repo
```bash
git clone https://github.com/jimnyone/GhostESP
cd GhostESP
```

### 2. Przełączenie na gałąź add-flipper-fap
```bash
git checkout add-flipper-fap
```

### 3. Instalacja zależności (jeśli nie masz jeszcze requests)
```bash
pip install requests
```

### 4. Uruchomienie build'u

#### Opcja A: PowerShell (rekomendowane na Windows)
```powershell
.\tools\build-windows.ps1
```

#### Opcja B: Bezpośrednio Python
```bash
python tools\build.py flipper
```

### 5. Wynik
Plik `.fap` zostanie wygenerowany w katalogu `dist/` jako `GhostESP-esp32-generic.fap`:
```
dist/
├── esp32-generic.zip          (pobierany dynamicznie)
├── esp32-generic.url          (link do assetu)
└── GhostESP-esp32-generic.fap (wygenerowany pakiet .fap)
```

## Co robi skrypt?

1. **Pobiera asset** — `esp32-generic.zip` z release v2.0 GhostESP-Revival
2. **Sprawdza rozmiar** — ostrzega, jeśli plik >100MB
3. **Generuje manifest** — `manifest.json` z metadanymi pakietu
4. **Pakuje .fap** — tworzy archiwum ZIP o nazwie `GhostESP-esp32-generic.fap`
5. **Czyszcze** — usuwa pliki tymczasowe

## Instalacja na Flipper Zero

Gotowy plik `.fap` możesz:
- Przenieść na SD kartę Flippera w odpowiedni katalog aplikacji
- Zainstalować za pomocą Flipper App Manager (jeśli obsługuje .fap)
- Rozpakować i zainstalować binaria ręcznie

## Nota o rozmiarze pliku

Jeśli pobierany asset będzie **większy niż 100 MB**:
- **Nie commituj** go bezpośrednio do repo (GitHub odrzuci plik >100MB)
- **Alternatywy**:
  1. Dodaj plik jako asset do Release w tym repo
  2. Skonfiguruj Git LFS (`git lfs install` + `.gitattributes`)
  3. Przechowaj plik poza repo i linkuj go w dokumentacji

Skrypt build.py automatycznie pobierze plik ze źródła (GitHub release'ów GhostESP-Revival), więc nie musisz go commitować — użytkownik uruchomi `build.py flipper` i wygeneruje sobie `.fap` lokalnie.

## Linki

- [GhostESP-Revival](https://github.com/GhostESP-Revival/GhostESP)
- [Release v2.0](https://github.com/GhostESP-Revival/GhostESP/releases/tag/v2.0)
- [esp32-generic.zip](https://github.com/GhostESP-Revival/GhostESP/releases/download/v2.0/esp32-generic.zip)

---

**Autor instrukcji:** GitHub Copilot Chat  
**Data:** 2026-07-02  
**Język:** Polski
