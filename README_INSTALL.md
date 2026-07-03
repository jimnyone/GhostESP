GhostESP Flipper installer — Windows

Pliki:
- install_flipper.ps1      : kopiuje ghostesp.fap na Flipper (apps\Tools\)
- uninstall_flipper.ps1    : usuwa ghostesp.fap z Flippera
- build_fap.ps1            : buduje ghostesp.fap z kodu źródłowego (wymaga toolchain)
- build_and_install.ps1    : buduje i kopiuje na Flippera (automatycznie próbuje wykryć mount)
- create_zip.ps1           : tworzy ZIP z katalogu ghostesp-flipper-installer

Wymagania (Windows):
- Git (opcjonalnie, do klonowania repo)
- Python >= 3.7 w PATH
- arm-none-eabi-gcc, cmake, make (dostępne z MSYS2, GNU Arm Embedded Toolchain, Chocolatey, lub instalatorów)
- Uprawnienie do odczytu/zapisu na punkcie montowania Flippera (dysk wymienny)
- (Opcjonalnie) pip dostępny i prawidłowo skonfigurowany

Szybkie użycie:
1) Zbuduj i zainstaluj (uruchom PowerShell jako zwykły użytkownik):
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
   .\build_and_install.ps1

2) Jeśli masz już ghostesp.fap:
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
   .\install_flipper.ps1 -LocalFile "C:\ścieżka\ghostesp.fap"

3) Odinstaluj:
   .\uninstall_flipper.ps1

Tworzenie ZIP:
   .\create_zip.ps1 -SourceDir ".\ghostesp-flipper-installer" -ZipFile ".\ghostesp-flipper-installer.zip"

Uwagi:
- Jeśli pobieranie z Releases zawiedzie, użyj -LocalFile z lokalnym plikiem .fap lub zbuduj go lokalnie.
- Budowanie wymaga zainstalowanego toolchainu ARM i podstawowych narzędzi (cmake, make). Na Windows najlepiej z MSYS2/MinGW lub oficjalnym GNU Arm Embedded.
- Używaj narzędzia zgodnie z prawem. GhostESP to narzędzie do badań bezpieczeństwa; użycie bez zgody może być nielegalne.
