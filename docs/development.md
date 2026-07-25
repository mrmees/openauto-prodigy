# Development Guide

## Build Environments

| Environment | Qt Version | Purpose |
|-------------|-----------|---------|
| **Raspberry Pi OS Trixie** (Pi 4) | Qt 6.8 | Target platform — run and verify here |
| **WSL2 / Debian Trixie** | Qt 6.8 | Dev builds, unit tests, cross-compiling |

Both environments run the same OS family and the same **system Qt** — no
`CMAKE_PREFIX_PATH`, no vendored Qt. Any Debian-Trixie-equivalent Linux works
for development. (An Ubuntu 24.04 / Qt 6.4 VM was used early in the project;
it is retired and dual-Qt-version compatibility is no longer a constraint.)

## Dependencies

### Raspberry Pi OS Trixie (Debian 13)

```bash
sudo apt install cmake g++ git pkg-config ccache \
  qt6-base-dev qt6-declarative-dev qt6-wayland \
  qt6-connectivity-dev qt6-multimedia-dev qt6-websockets-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavformat-dev libavcodec-dev libavutil-dev \
  libpipewire-0.3-dev libspa-0.2-dev \
  libyaml-cpp-dev libsystemd-dev libbluetooth-dev \
  hostapd rfkill bluez \
  pipewire-pulse pulseaudio-utils \
  python3-flask python3-dbus-next python3-yaml python3-venv \
  redsocks iptables inotify-tools swaybg udisks2 \
  qt6-webengine-dev qml6-module-qtwebengine  # optional — web widget runtime; builds fine without
```

Note: `libbluetooth-dev` (raw BlueZ headers, `bluetooth/bluetooth.h`) is required in addition to `qt6-connectivity-dev` — `BluetoothDiscoveryService` uses BlueZ sockets/SDP directly. `install.sh` already includes it.

Note: `libsystemd-dev` enables the `sd_notify` readiness and watchdog support
used by the installed `Type=notify` service. CMake can build without it for a
standalone development process, but Pi service installs require the integration.

Note: `udisks2` is a Pi **runtime** dependency, not a build dep — it ships no
headers/libraries to link against. `UsbMediaWatcher` talks to its D-Bus
service to detect and mount USB media; the polkit grant that lets the
service user drive it without a password lives in
`config/udisks-polkit.rules` (installed by `install.sh` alongside the BlueZ
agent rule).

### WSL2 Debian Trixie (dev box)

Use the same build dependencies. Pi-only runtime packages such as `hostapd`,
`rfkill`, `bluez`, `pipewire-pulse`, `redsocks`, `iptables`, `swaybg`, and
`udisks2` are not needed for unit tests on a dev box. Docker (`docker.io`)
enables `cross-build.sh` for Pi binaries.

If the repo checkout lives on a Windows drive (`/mnt/...`), keep the CMake
build directory on the Linux filesystem — object churn through the 9p mount
makes in-repo builds painfully slow. See `AGENTS.md` → Commands.

## Building

```bash
git clone --recurse-submodules https://github.com/mrmees/openauto-prodigy.git
cd openauto-prodigy
cmake -S . -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy -j$(nproc)
```

**Important:** Don't forget `--recurse-submodules` — the [open-android-auto](https://github.com/mrmees/open-android-auto) protobuf definitions are a git submodule under `libs/prodigy-oaa-protocol/proto/`.

### Interactive Install (RPi OS Trixie)

For a fresh Raspberry Pi, the install script handles everything:

```bash
git clone --recurse-submodules https://github.com/mrmees/openauto-prodigy.git
cd openauto-prodigy
bash install.sh
```

Run source mode only from the complete Git checkout that contains `install.sh`.
The installer resolves that physical checkout and uses it as the source and
build root, so it may live outside the user's home directory. Standard-input
execution and a standalone copied script are rejected before privilege
escalation or system mutation; clone the repository and run the checked-out
script instead.

`install.sh` provides an install mode picker:
- Build locally from source.
- Download a precompiled release from GitHub.

Both install modes run platform validation first (Debian/RPi OS family, ARM architecture, and Pi 4 hardware model, with explicit continue prompts if mismatched).

To list available precompiled release assets without installing:

```bash
bash install.sh --list-prebuilt
```

The source mode installs dependencies, builds from source, generates config,
installs the canonical `restart.sh` in the checkout root, and creates systemd
services. Both source and prebuilt application units use `Type=notify`; their
blocking systemd start/restart jobs complete at the application's `READY=1`
boundary. Both install the byte-identical
`config/installer/openauto-preflight` helper and render
`config/systemd/openauto-prodigy.service.in` for the selected user, UID, and
install root. The unit orders after Bluetooth and PipeWire. Its bounded
Wayland `ExecCondition` skips a start cleanly when the compositor socket is
absent; a later explicit start proceeds normally once the socket exists.

Package and build commands run in process groups owned by the installer. Normal
completion, command failure, or interruption terminates and reaps only those
owned groups. If the application service was active before an in-place source
rebuild, it is stopped before configure/link work and restored when the
installer succeeds or exits with an error; an inactive service remains
inactive, and declining a rebuild does not stop it.

### Prebuilt Install (RPi OS Trixie)

If you have a packaged prebuilt release tarball, install without building:

```bash
tar -xzf openauto-prodigy-prebuilt-<tag>-pi4-aarch64.tar.gz
cd openauto-prodigy-prebuilt-<tag>-pi4-aarch64
bash install-prebuilt.sh
```

The prebuilt installer validates and stages the complete managed payload before
stopping any service. It records the active state of the application,
web-config, and privileged system service, swaps only the managed payload paths,
and keeps the prior payload and unit files until the replacement services have
regained exactly that state. Any post-stop failure restores the prior bytes and
active set. Services that were inactive remain inactive; installing a unit does
not start it as a side effect.

To create a prebuilt package from this repo:

```bash
./cross-build.sh -DCMAKE_BUILD_TYPE=Release
./tools/package-prebuilt-release.sh --build-dir build-pi --version-tag <tag>
```

`cross-build.sh` defaults to building only the app target (`openauto-prodigy`),
which is all a Pi deploy or package needs. Its high-churn CMake and object cache
lives in a persistent Docker volume keyed by the host UID and canonical checkout
path; builds using the same cache are serialized. Only the final binary is
copied to `build-pi/src/openauto-prodigy`. This avoids writing the cross-build
tree through the Windows/9p source mount without sharing stale objects across
clones or worktrees. Fast and full modes share a checkout publication lock;
deterministically named containers are cleaned up after an interrupted build so
an orphan cannot keep mutating the cache. Locks live in a validated, mode-0700
per-UID directory beneath `XDG_RUNTIME_DIR` (or `/tmp` as a fallback), and each
fast build repairs the cache-volume root ownership before configuring so an
interrupted first run cannot wedge the cache. Use `--reset-cache` to recreate
that app-only cache. Pass `--full` to retain the legacy host-visible build tree
and also build the ARM test binaries (~20 min).

Prebuilt release convention:
- Asset: `openauto-prodigy-prebuilt-<tag>-pi4-aarch64.tar.gz`
- Root dir: `openauto-prodigy-prebuilt-<tag>-pi4-aarch64/`
- Metadata: `RELEASE.json` at archive root

## Running

### On Raspberry Pi (Wayland/labwc)

```bash
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
./src/openauto-prodigy
```

The Pi runs labwc as its Wayland compositor. Do **not** use `eglfs` — it won't work with this display server setup.

### Via SSH

```bash
ssh <pi-user>@<pi-host> 'sudo systemctl restart openauto-prodigy.service'
# Equivalent helper from an installed checkout (systemd remains the owner):
ssh <pi-user>@<pi-host> 'cd openauto-prodigy && ./restart.sh'
# Forced recovery stops the unit, cleans exact-identity legacy orphans, and starts once:
ssh <pi-user>@<pi-host> 'cd openauto-prodigy && ./restart.sh --force-kill'
# Validate installed pre-flight requirements:
ssh <pi-user>@<pi-host> 'sudo openauto-preflight --check-only'
# Inspect service state:
ssh <pi-user>@<pi-host> 'systemctl status openauto-prodigy.service --no-pager'
```

### Logs

```bash
# systemd service (normal operation)
journalctl -u openauto-prodigy.service -n 200
```

The helper does not create a detached process or a separate log file. Normal
restarts are systemd-only. Forced recovery additionally checks for an
upgrade-era unmanaged process by the unit's exact executable path and user UID
after the service is stopped; it excludes service-cgroup members and unrelated
same-name processes before bounded TERM/KILL cleanup. The replacement is still
started exactly once by `openauto-prodigy.service`, so logs remain in the
journal and `MainPID` identifies the sole app process.

### Web Config Panel

```bash
cd web-config && python3 server.py
# Access at http://<pi-ip>:8080  (port override: OAP_WEB_PORT)
```

Or via the systemd service created by `install.sh`:
```bash
sudo systemctl start openauto-prodigy-web
```

## Running Tests

```bash
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```

**ctest does NOT compile the app target** — always build `openauto-prodigy`
explicitly (`cmake --build . --target openauto-prodigy`) before claiming
green; a broken `main.cpp` is invisible to the test suite.

See `tests/README.md` for suite layout and single-test invocation.

## Qt / QML Gotchas

Code-level traps are documented next to the code — read the nearest agent
file before touching a subsystem: `src/AGENTS.md` (Qt, D-Bus, PipeWire),
`src/core/aa/AGENTS.md` (AA protocol), `qml/AGENTS.md` (UI). Build-system
gotchas not covered there:

### QML Resource Paths

QML loads via `engine.load()` with `qrc:/OpenAutoProdigy/...` URLs — the
project pins this resource prefix in `qt_add_qml_module` (not the Qt 6.5+
default `qrc:/qt/qml/OpenAutoProdigy/`). Use the existing prefix for any new
QML component references.

### QML Subdirectory Types

Files in QML subdirectories (e.g., `qml/applications/home/HomeMenu.qml`) need the `QT_QML_SOURCE_TYPENAME` property set in CMake to flatten the type name:

```cmake
set_source_files_properties(qml/applications/home/HomeMenu.qml
    PROPERTIES QT_QML_SOURCE_TYPENAME HomeMenu)
```

### QSettings Forward Declaration

Don't forward-declare `class QSettings` inside a namespace — it creates `yournamespace::QSettings` instead of `::QSettings`. Always `#include <QSettings>`.

### QtQuick.Controls Runtime

The `qml6-module-qtqml-workerscript` package is required at runtime for QtQuick.Controls to load. Without it you'll get mysterious QML import failures.

## Project Structure

Top-level map only — the component-level view (plugins, services, threading,
data flow) is maintained in [architecture.md](architecture.md):

```
openauto-prodigy/
├── src/                        # C++ app — core services, AA protocol, plugins, UI controllers
├── qml/                        # QML UI (ships inside the binary — see qml/AGENTS.md)
├── libs/prodigy-oaa-protocol/  # AA protocol library (proto defs via git submodule)
├── proto/api/                  # External API v1 contract (frozen, additive-only)
├── web-config/                 # Flask web configuration panel
├── tests/                      # CTest + Qt Test suite (tests/README.md)
├── tools/                      # Dev and release tooling (tools/README.md)
├── scripts/                    # Repo scripts — review gate, link checker (scripts/README.md)
└── docs/                       # Documentation (docs/INDEX.md is the map)
```

### External API v1

TCP (`9810`) + WebSocket (`9811`) protobuf server for companion app, in-process web widgets, and third-party clients — implemented in `src/core/api/`. Design doc: `docs/archive/plans/2026-07-06-external-api-v1-design.md`. Config keys live under `api.*` in `config.yaml` (enable, ports, LAN exposure, pairing/queue timeouts).

## Debugging Tips

### BlueZ D-Bus Debugging

```bash
# List all BlueZ managed objects
busctl call org.bluez / org.freedesktop.DBus.ObjectManager GetManagedObjects

# Monitor D-Bus signals
dbus-monitor --system "sender='org.bluez'"

# Check paired devices
bluetoothctl devices
```

### Phone Behavior

Phone state can degrade after rapid connect/disconnect cycles during development. If the phone stops responding normally:
1. Clear Android Auto app cache/data on phone
2. Reboot the phone
3. Re-pair Bluetooth

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for the branch flow and PR checklist, and [debugging-notes.md](how-to/debugging-notes.md) for known issues and discoveries. The most helpful thing right now is testing with different phones and reporting connection behavior.
