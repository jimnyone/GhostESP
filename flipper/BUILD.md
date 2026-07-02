# Build Instructions for Flipper Zero Application

## Quick Build

```bash
# Clone repository
git clone https://github.com/jimnyone/GhostESP.git
cd GhostESP

# Build Flipper app
cd flipper
mkdir build && cd build
cmake -GNinja ..
ninja
```

## GitHub Actions

The workflow automatically builds the Flipper app on:
- ✅ Push to `Development-deki`, `main`, or `master`
- ✅ Pull requests to these branches
- ✅ Release creation
- ✅ Manual trigger (workflow_dispatch)

### Artifacts

Built `.fap` files are available:
- In GitHub Actions run artifacts (30-day retention)
- In Release assets (permanent)

## Manual Build (Local)

### Prerequisites
- ARM GCC: `arm-none-eabi-gcc`
- CMake: `cmake`
- Ninja: `ninja-build`
- Docker (optional)

### Using Docker

```bash
docker run --rm -v $(pwd):/src flipperdevices/flipperzero-dev:latest bash -c "
  cd /src/flipper
  cmake -B build -GNinja
  ninja -C build
"
```

### Linux/Mac

```bash
cd flipper
cmake -B build -GNinja
ninja -C build
```

The `.fap` file will be in `flipper/build/`.

## Installation

Copy the built `.fap` to Flipper Zero:

1. Mount Flipper as USB drive
2. Navigate to `apps/Tools/`
3. Copy `ghostesp.fap`
4. Unmount and run from Flipper menu

## Troubleshooting

### Build fails: "CMakeLists.txt not found"

Ensure you're in the correct directory:
```bash
ls -la flipper/CMakeLists.txt  # Should exist
```

### Missing dependencies

Install Flipper SDK:
```bash
# Ubuntu/Debian
sudo apt-get install cmake ninja-build arm-none-eabi-gcc

# macOS
brew install cmake ninja arm-none-eabi-gcc
```

### Permission denied on .fap file

```bash
chmod +x flipper/build/ghostesp.fap
```

## Environment Variables

- `BUILD_TYPE`: `Release` (default) or `Debug`
- `FLIPPER_TARGET`: Target board (auto-detected)

## Release Process

1. Create a tag: `git tag v2.0.1`
2. Push tag: `git push origin v2.0.1`
3. Create GitHub Release
4. Workflow automatically builds and uploads `.fap`

---

For more info: https://docs.ghostesp.net
