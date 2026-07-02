# GitHub Actions Configuration for GhostESP Flipper Build

## Szybka konfiguracja (5 minut)

### 1. Ustawienie Discord Webhooks (opcjonalnie)

Jeśli chcesz otrzymywać powiadomienia na Discord o statusie build:

#### Na Discordzie:
1. Otwórz swój serwer Discord
2. Idź do **Settings** → **Integrations** → **Webhooks**
3. Kliknij **New Webhook**
4. Nazwij go `GhostESP-CI`
5. Skopiuj **Webhook URL**

#### Na GitHubie:
1. Idź do **Settings** → **Secrets and variables** → **Actions**
2. Kliknij **New repository secret**
3. Nazwa: `DISCORD_WEBHOOK`
4. Wartość: Wklej skopiowany Webhook URL
5. Kliknij **Add secret**

### 2. Automatyczne budowanie jest gotowe!

Workflow zostanie uruchomiony automatycznie gdy:
- ✅ Pushjesz do gałęzi `Development-deki`, `main`, lub `master`
- ✅ Tworzy się Pull Request
- ✅ Publikujesz Release
- ✅ Ręcznie uruchamiasz workflow

## Monitorowanie build'ów

### W GitHub

1. Idź do **Actions** w swoim repozytorium
2. Obserwuj ostatni build w liście
3. Kliknij aby zobaczyć szczegóły
4. Pobierz artifact `flipper-app` zawierający `ghostesp.fap`

### Na Discordzie

Jeśli skonfigurowałeś webhook, będziesz otrzymywać:
- ✅ **Sukces**: Zielone powiadomienie z linkiem do build
- ❌ **Błąd**: Czerwone powiadomienie z komunikatem błędu

## Release Process

### Automatyczne budowanie dla Release

```bash
# 1. Utwórz tag
git tag v2.0.1

# 2. Push tag do GitHub
git push origin v2.0.1

# 3. W GitHub: Create Release from tag
#    - Idź do "Releases"
#    - Kliknij "Create a release"
#    - Wybierz tag v2.0.1
#    - Dodaj description
#    - Kliknij "Publish release"

# Workflow automatycznie:
# ✅ Zbuduje aplikację
# ✅ Przesle ghostesp.fap do Release assets
```

## Struktura plików

```
.github/
└── workflows/
    └── flipper-build.yml          # Workflow CI/CD
flipper/
├── BUILD.md                       # Instrukcje budowania
├── CMakeLists.txt                 # (jeśli istnieje)
└── ...inne pliki aplikacji...
FLIPPER_INSTALLATION.md            # Instrukcje instalacji (PL)
```

## Zmienne środowiskowe workflow

Możesz dostosować workflow edytując `.github/workflows/flipper-build.yml`:

```yaml
# Gałęzie do budowania
branches:
  - Development-deki
  - main
  - master

# Ścieżki które triggerują build
paths:
  - 'flipper/**'
  - '.github/workflows/flipper-build.yml'
```

## Troubleshooting

### Build się nie uruchamia

**Sprawdzenie:**
1. Idź do **Actions** w GitHub
2. Jeśli workflow nie widać, upewnij się że plik jest w `.github/workflows/flipper-build.yml`
3. Skopiuj plik z gałęzi `Development-deki` jeśli go tam nie ma

### Build się uruchamia ale zwraca błąd

```
❌ CMakeLists.txt not found
```

**Rozwiązanie:**
- Upewnij się że `flipper/CMakeLists.txt` istnieje
- Workflow szuka pliku w katalogu `flipper/`

```
❌ Build failed: ninja not found
```

**Rozwiązanie:**
- Workflow używa Docker z `flipperdevices/flipperzero-dev:latest`
- Wszystkie zależności są już zainstalowane
- Sprawdź czy `flipper/CMakeLists.txt` jest prawidłowy

### Discord webhook nie wysyła powiadomień

**Sprawdzenie:**
1. Secret `DISCORD_WEBHOOK` jest ustawiony? (Settings → Secrets)
2. Webhook URL jest prawidłowy?
3. Sprawdź logi workflow - sekcja "Send Discord Notification"

**Jeśli Discord notifications nie są potrzebne:**
- Możesz bezpiecznie pominąć konfigurację webhook
- Workflow będzie działał bez powiadomień

## Pobieranie artifacts

### Ze strony GitHub

1. Idź do **Actions**
2. Kliknij na ostatni successful build
3. Przewiń do dołu - sekcja **Artifacts**
4. Pobierz `flipper-app`
5. Rozpakuj i weź `ghostesp.fap`

### Programowo (CLI)

```bash
# Zainstaluj GitHub CLI
# https://cli.github.com/

# Pobierz ostatni artifact
gh run list --limit 1 --workflow flipper-build.yml
# Skopiuj run ID (np. 1234567890)

gh run download 1234567890 -n flipper-app
```

## Dostosowanie workflow

### Dodaj własne kroki

Edytuj `.github/workflows/flipper-build.yml` i dodaj kroki:

```yaml
- name: Custom Step
  run: |
    echo "Mój niestandardowy krok"
    ./scripts/custom-build.sh
```

### Zmień kontener Docker

```yaml
container:
  image: flipperdevices/flipperzero-dev:latest  # Zmień tag wersji
```

### Wysyłaj na inny serwis

Zamiast GitHub Releases możesz wysyłać do:
- AWS S3
- Google Drive
- Own server via SSH
- itp.

## Bezpieczeństwo

- ✅ Secrets (`DISCORD_WEBHOOK`) są szyfrowane
- ✅ Artifact'y są przechowywane przez 30 dni
- ✅ Tylko repozytorium može trigger'ować workflow
- ✅ Nie są przechowywane żadne dane osobowe

## Wsparcie i odnośniki

- 📖 GitHub Actions docs: https://docs.github.com/actions
- 🔧 Flipper Zero dev: https://docs.flipperzero.one/development
- 💬 GhostESP Discord: https://discord.gg/5cyNmUMgwh
- 🐛 Issues: https://github.com/jimnyone/GhostESP/issues

---

## Checklist - Gotowy setup

- [ ] Plik `.github/workflows/flipper-build.yml` istnieje
- [ ] Plik `flipper/BUILD.md` istnieje
- [ ] Plik `flipper/CMakeLists.txt` istnieje (lub Makefile)
- [ ] (Opcjonalnie) Discord Webhook skonfigurowany
- [ ] Pierwszy push do Development-deki - workflow uruchomiony ✅

**Gotowe! 🎉 Teraz każdy push będzie automatycznie budować aplikację Flippera.**
