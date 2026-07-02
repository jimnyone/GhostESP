# Instalacja GhostESP na Flipper Zero

Kompletny przewodnik instalacji i konfiguracji aplikacji **GhostESP** do **Flipper Zero**.

## 📋 Spis treści

- [Wymagania](#wymagania)
- [Szybki start](#szybki-start)
- [Instalacja ręczna](#instalacja-ręczna)
- [Połączenie z ESP32](#połączenie-z-esp32)
- [Rozwiązywanie problemów](#rozwiązywanie-problemów)
- [Funkcje](#funkcje)
- [Zaawansowana konfiguracja](#zaawansowana-konfiguracja)

---

## Wymagania

### Hardware
- ✅ **Flipper Zero** (najnowsza wersja firmware)
- ✅ **Urządzenie ESP32** z zainstalowanym **GhostESP v2.0+**
- ✅ **Kabel USB** (do Flippera) lub **połączenie Bluetooth** (jeśli dostępne)

### Software
- Flipper Zero firmware: **v1.0+**
- GhostESP firmware: **v2.0+**

### Wsparcie ESP32
Aplikacja Flippera obsługuje następujące urządzenia ESP32:

| Urządzenie | Wsparcie WiFi | Wsparcie BLE | Wsparcie SubGHz |
|-----------|:-------------:|:------------:|:---------------:|
| ESP32 (Wroom) | ✅ | ✅ | ❌ |
| ESP32-S3 | ✅ | ✅ | ❌ |
| ESP32-C5 | ✅ | ✅ | ✅ |
| ESP32-C6 | ✅ | ✅ | ✅ |

---

## Szybki start

### 1. Pobierz najnowszą aplikację

Opcja A: **Pre-built aplikacja** (zalecane)
- Przejdź do [GitHub Releases](https://github.com/jimnyone/GhostESP/releases)
- Pobierz `ghostesp.fap` (Flipper Application Package)

Opcja B: **Aplikacja z App Store Flippera**
- Otwórz **Flipper Zero App Store**
- Wyszukaj "GhostESP"
- Naciśnij **Zainstaluj**

### 2. Zainstaluj aplikację

#### Na Flipper Zero:

1. **Podłącz Flippera** do komputera kablem USB
2. **Montuj** jako dysk wymiennym w systemie operacyjnym
3. **Utwórz katalog** (jeśli nie istnieje): `apps/Tools/`
4. **Skopiuj plik** `ghostesp.fap` do folderu `apps/Tools/`
5. **Odmontuj dysk** i odłącz Flippera

#### Na Flipper Zero (za pośrednictwem aplikacji):

1. Otwórz **Applications** na Flipper Zero
2. Przejdź do **Tools**
3. Wyszukaj **GhostESP**
4. Naciśnij ✅ aby zainstalować

### 3. Uruchom aplikację

1. Na Flipper Zero: **Applications** → **Tools** → **GhostESP**
2. Aplikacja uruchomi się i będzie gotowa do połączenia

---

## Instalacja ręczna

### Budowanie z kodu źródłowego

Jeśli chcesz zbudować aplikację samodzielnie:

#### Wymagania
- `git`
- `arm-none-eabi-gcc` (ARM GCC compiler)
- `cmake`
- `make`
- Python 3.7+

#### Kroki

```bash
# 1. Sklonuj repozytorium GhostESP
git clone https://github.com/jimnyone/GhostESP.git
cd GhostESP

# 2. Zainstaluj zależności
pip install -r requirements.txt

# 3. Zbuduj aplikację Flippera
python build.py flipper

# 4. Plik jest teraz tutaj
ls -la dist/ghostesp.fap
```

Gotowy plik `.fap` znajduje się w folderze `dist/`.

---

## Połączenie z ESP32

### Metoda 1: Bluetooth (zalecane)

#### Konfiguracja na ESP32

1. **Upewnij się**, że na twoim ESP32 jest zainstalowany **GhostESP v2.0+**
2. **Flasz firmware'u** ESP32: https://flasher.ghostesp.net/

#### Na Flipper Zero

1. Otwórz **GhostESP** aplikację
2. Przejdź do **Connection** (Połączenie)
3. Wybierz **Bluetooth**
4. Aplikacja skanuje dostępne urządzenia
5. Wybierz swoje urządzenie ESP32 (np. `GhostESP-Marauder`)
6. Potwierdź połączenie PIN (domyślnie `0000`)

### Metoda 2: UART (Serial)

#### Sprzęt

Połącz Flippera z ESP32 poprzez:
- **RX Flippera** (GPIO 13) → **TX ESP32** (GPIO 1)
- **TX Flippera** (GPIO 12) → **RX ESP32** (GPIO 3)
- **GND** → **GND**

#### Na Flipper Zero

1. Otwórz **GhostESP** aplikację
2. Przejdź do **Connection**
3. Wybierz **UART**
4. Wybierz port (domyślnie `/dev/uart1`)
5. Potwierdź

### Metoda 3: WiFi

#### Na Flipper Zero

1. Otwórz **GhostESP** aplikację
2. Przejdź do **Connection**
3. Wybierz **WiFi**
4. Wpisz IP adres ESP32
5. Potwierdź

> **Uwaga:** Flipper Zero nie ma wbudowanego WiFi, więc będziesz musiał użyć dodatkowego modułu WiFi lub połączenia poprzez proxy.

---

## Funkcje

Po podłączeniu do ESP32, będziesz mieć dostęp do:

### 🔗 WiFi
- Skanowanie sieci WiFi
- Przechwytywanie handshake'u WPA/WPA2
- Ataki deauth (rozłączania)
- Evil Portal (fałszywy dostęp do Internetu)
- Analiza kanałów

### 📡 Bluetooth (BLE)
- Skanowanie urządzeń BLE
- Detekcja AirTag
- BLE spam
- Wyszukiwanie Flipper Zero

### 🛰️ GPS
- Wardriving (zbieranie danych WiFi/BLE z lokalizacją GPS)
- Eksport do WiGLE
- Zapisanie historii GPS

### 🔧 Konfiguracja
- Zmiana ustawień ESP32
- Kontrola informacji o baterii
- Ustawienia wyświetlacza

---

## Rozwiązywanie problemów

### Problem: Aplikacja się nie uruchamia

**Rozwiązanie:**
```
1. Upewnij się, że masz najnowszy firmware Flippera
   - Flipper Zero → Settings → Firmware → Check for updates

2. Usuń i przeinstaluj aplikację
   - Usuń ghostesp.fap z apps/Tools/
   - Pobierz najnowszą wersję z GitHub Releases
   - Przeinstaluj
```

### Problem: Nie mogę połączyć się z ESP32

**Rozwiązanie Bluetooth:**
```
1. Sprawdź, czy ESP32 ma włączone Bluetooth
   - Na ESP32: blescan -f (sprawdź wyjście)

2. Wyczyść parowanie
   - Na Flipper Zero: Settings → Bluetooth → Forget all devices
   - Spróbuj ponownie

3. Zrestartuj oba urządzenia
   - ESP32: wciśnij przycisk RESET
   - Flipper Zero: przytrzymaj i wymuszaj restart
```

**Rozwiązanie UART:**
```
1. Sprawdź połączenie fizyczne
   - RX/TX/GND są prawidłowo połączone?

2. Sprawdź szybkość transmisji
   - Upewnij się, że obie strony używają 115200 baud

3. Przetestuj połączenie
   - Na ESP32 terminal: At /dev/ttyUSB0 115200
```

### Problem: "Connection timeout"

**Rozwiązanie:**
```
1. Zwiększ timeout w ustawieniach
   - GhostESP → Connection → Settings → Timeout: 30 sekund

2. Sprawdź zasięg (jeśli Bluetooth)
   - Przysuń urządzenia bliżej (max 10 metrów)

3. Wyłącz inne urządzenia Bluetooth
   - Mogą powodować zakłócenia
```

### Problem: Aplikacja zamyka się lub się zawiesza

**Rozwiązanie:**
```
1. Sprawdź logi
   - Flipper Zero → Apps → GhostESP → View logs

2. Wyczyść cache
   - Flipper Zero → Settings → Storage → Clear cache

3. Użyj starszej wersji aplikacji
   - Jeśli najnowsza wersja ma błędy
```

---

## Zaawansowana konfiguracja

### Zmiana portów I/O

Jeśli chcesz używać różnych pinów GPIO do UART:

1. Edytuj plik konfiguracyjny na Flipper Zero
2. Przejdź do: `.config/apps/ghostesp.conf`
3. Zmień wartości:
   ```
   UART_RX_PIN=13
   UART_TX_PIN=12
   UART_BAUD=115200
   ```

### Zmiana szybkości transmisji

Na ESP32:
```
uart_set_baud 115200
```

Na Flipper Zero (config):
```
UART_BAUD=115200
```

### Sesja zdalna (Remote)

Aby kontrolować ESP32 z innego Flippera:

1. Włącz tryb serwera na głównym Flipper Zero
   - GhostESP → Remote → Server mode

2. Na drugim Flipper Zero:
   - GhostESP → Remote → Connect to
   - Wpisz IP pierwszego Flippera

---

## Wsparcie i społeczność

- 💬 **Discord**: https://discord.gg/5cyNmUMgwh
- 📖 **Dokumentacja**: https://docs.ghostesp.net
- 🐛 **Zgłoś błąd**: https://github.com/jimnyone/GhostESP/issues
- 🌐 **Strona**: https://ghostesp.net

---

## Licencja

GhostESP jest licencjonowany na licencji **GPL-3.0**.
Dla szczegółów zobacz plik [LICENSE](LICENSE).

---

## Wyłączenie odpowiedzialności

GhostESP jest narzędziem edukacyjnym do badań bezpieczeństwa. Niedozwolone użycie bez zgody jest nielegalne. Zawsze upewnij się, że masz autoryzację przed testowaniem bezpieczeństwa sieci.
