# Infrastructure Improvements Implementation Plan
> **Status:** COMPLETED

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make OpenAuto Prodigy hardware-portable by auto-detecting touch input, persisting WiFi AP config, and enabling boot-to-app.

**Architecture:** Three independent features: (1) InputDeviceScanner utility scans `/dev/input/` for `INPUT_PROP_DIRECT` touchscreens, replacing hardcoded device path; (2) Install script writes systemd-networkd + hostapd configs from user-selected wireless interface; (3) Cleaned-up systemd service with opt-in auto-start. All config values read via existing ConfigService/YamlConfig.

**Tech Stack:** C++17, Linux evdev ioctls, Qt 6, systemd-networkd, hostapd, bash

---

## Context: Key Files

- **Touch hardcode:** `src/plugins/android_auto/AndroidAutoPlugin.cpp:58` — `"/dev/input/event4"`
- **BT discovery hardcode:** `src/core/aa/BluetoothDiscoveryService.cpp:145,246` — `"wlan0"`
- **Config reading:** `src/core/services/IConfigService.hpp` — `value(key)` returns `QVariant`
- **Tests:** `tests/CMakeLists.txt` — add new test targets here
- **Build:** `src/CMakeLists.txt` — add new .cpp files here
- **Install script:** `install.sh` — network setup + service creation

## Task 1: InputDeviceScanner — Header + Test

**Files:**
- Create: `src/core/InputDeviceScanner.hpp`
- Create: `tests/test_input_device_scanner.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test**

Create `tests/test_input_device_scanner.cpp`:

```cpp
#include <QTest>
#include "core/InputDeviceScanner.hpp"

class TestInputDeviceScanner : public QObject {
    Q_OBJECT

private slots:
    void findTouchDevice_returnsStringPath()
    {
        // On dev VM there may or may not be a touchscreen.
        // Just verify the function runs without crashing and returns a QString.
        QString result = oap::InputDeviceScanner::findTouchDevice();
        // result is either empty (no touchscreen) or a /dev/input/eventN path
        if (!result.isEmpty()) {
            QVERIFY(result.startsWith("/dev/input/event"));
        }
    }

    void listInputDevices_returnsVector()
    {
        auto devices = oap::InputDeviceScanner::listInputDevices();
        // Should return some devices on any Linux system
        // Each entry has path and name
        for (const auto& dev : devices) {
            QVERIFY(dev.path.startsWith("/dev/input/event"));
            // name may be empty if permission denied, that's ok
        }
    }
};

QTEST_MAIN(TestInputDeviceScanner)
#include "test_input_device_scanner.moc"
```

**Step 2: Write the header**

Create `src/core/InputDeviceScanner.hpp`:

```cpp
#pragma once

#include <QString>
#include <QVector>

namespace oap {

class InputDeviceScanner {
public:
    struct DeviceInfo {
        QString path;   // e.g. "/dev/input/event4"
        QString name;   // e.g. "DFRobot USB Multi Touch"
        bool isTouchscreen = false;  // has INPUT_PROP_DIRECT
    };

    /// Scan /dev/input/event0..event31 and return info for each accessible device.
    static QVector<DeviceInfo> listInputDevices();

    /// Find first device with INPUT_PROP_DIRECT capability.
    /// Returns empty string if none found.
    static QString findTouchDevice();
};

} // namespace oap
```

**Step 3: Add test to CMakeLists.txt**

Add to `tests/CMakeLists.txt` at the end (before the `enable_testing()` line):

```cmake
add_executable(test_input_device_scanner
    test_input_device_scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/core/InputDeviceScanner.cpp
)
target_include_directories(test_input_device_scanner PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_input_device_scanner PRIVATE
    Qt6::Core
    Qt6::Test
)
```

And add to the test list:

```cmake
add_test(NAME test_input_device_scanner COMMAND test_input_device_scanner)
```

**Step 4: Run test to verify it fails**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake .. && cmake --build . -j$(nproc) 2>&1
```

Expected: Build FAILS — `InputDeviceScanner.cpp` doesn't exist yet.

**Step 5: Commit**

```bash
git add tests/test_input_device_scanner.cpp src/core/InputDeviceScanner.hpp tests/CMakeLists.txt
git commit -m "test: add InputDeviceScanner test and header"
```

---

## Task 2: InputDeviceScanner — Implementation

**Files:**
- Create: `src/core/InputDeviceScanner.cpp`
- Modify: `src/CMakeLists.txt:1-31` — add to source list

**Step 1: Write the implementation**

Create `src/core/InputDeviceScanner.cpp`:

```cpp
#include "InputDeviceScanner.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <cstring>

// INPUT_PROP_DIRECT might not be defined on older headers
#ifndef INPUT_PROP_DIRECT
#define INPUT_PROP_DIRECT 0x01
#endif

namespace oap {

QVector<InputDeviceScanner::DeviceInfo> InputDeviceScanner::listInputDevices()
{
    QVector<DeviceInfo> devices;

    for (int i = 0; i < 32; ++i) {
        QString path = QStringLiteral("/dev/input/event%1").arg(i);

        int fd = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        DeviceInfo info;
        info.path = path;

        // Read device name
        char nameBuf[256] = {};
        if (::ioctl(fd, EVIOCGNAME(sizeof(nameBuf)), nameBuf) >= 0) {
            info.name = QString::fromUtf8(nameBuf);
        }

        // Check INPUT_PROP_DIRECT (touchscreen)
        unsigned long propBits[(INPUT_PROP_CNT + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long))] = {};
        if (::ioctl(fd, EVIOCGPROP(sizeof(propBits)), propBits) >= 0) {
            // Check if bit INPUT_PROP_DIRECT is set
            if (propBits[INPUT_PROP_DIRECT / (8 * sizeof(unsigned long))]
                & (1UL << (INPUT_PROP_DIRECT % (8 * sizeof(unsigned long))))) {
                info.isTouchscreen = true;
            }
        }

        ::close(fd);
        devices.append(info);
    }

    return devices;
}

QString InputDeviceScanner::findTouchDevice()
{
    auto devices = listInputDevices();
    for (const auto& dev : devices) {
        if (dev.isTouchscreen)
            return dev.path;
    }
    return {};
}

} // namespace oap
```

**Step 2: Add to src/CMakeLists.txt**

Add `core/InputDeviceScanner.cpp` to the `qt_add_executable` source list in `src/CMakeLists.txt:1-31`, after the existing `core/` entries (e.g. after line 5 `core/YamlConfig.cpp`):

```
    core/InputDeviceScanner.cpp
```

**Step 3: Build and run tests**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure
```

Expected: All tests pass (including the new `test_input_device_scanner`).

**Step 4: Commit**

```bash
git add src/core/InputDeviceScanner.cpp src/CMakeLists.txt
git commit -m "feat: implement InputDeviceScanner with INPUT_PROP_DIRECT detection"
```

---

## Task 3: Wire touch auto-discovery into AndroidAutoPlugin

**Files:**
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp:57-68`

**Step 1: Replace the hardcoded touch device path**

In `src/plugins/android_auto/AndroidAutoPlugin.cpp`, replace lines 57-68:

```cpp
    // Start evdev touch reader if device exists (Pi only, not dev VM)
    QString touchDevice = QStringLiteral("/dev/input/event4");  // TODO: configurable
    if (QFile::exists(touchDevice)) {
        touchReader_ = new oap::aa::EvdevTouchReader(
            aaService_->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range
            1280, 720,    // AA touch coordinate space
            1024, 600,    // physical display resolution
            this);
        touchReader_->start();
    }
```

With:

```cpp
    // Auto-detect or read touch device from config
    QString touchDevice;
    if (hostContext_ && hostContext_->configService()) {
        touchDevice = hostContext_->configService()->value("touch.device").toString();
    }
    if (touchDevice.isEmpty()) {
        touchDevice = oap::InputDeviceScanner::findTouchDevice();
        if (!touchDevice.isEmpty()) {
            BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Auto-detected touch device: "
                                    << touchDevice.toStdString();
        }
    }

    // Read display resolution from config (default: 1024x600)
    int displayW = 1024, displayH = 600;
    if (hostContext_ && hostContext_->configService()) {
        QVariant w = hostContext_->configService()->value("display.width");
        QVariant h = hostContext_->configService()->value("display.height");
        if (w.isValid()) displayW = w.toInt();
        if (h.isValid()) displayH = h.toInt();
    }

    if (!touchDevice.isEmpty() && QFile::exists(touchDevice)) {
        touchReader_ = new oap::aa::EvdevTouchReader(
            aaService_->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range (overridden by EVIOCGABS in reader)
            1280, 720,    // AA touch coordinate space (matches video resolution)
            displayW, displayH,
            this);
        touchReader_->start();
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Touch: " << touchDevice.toStdString()
                                << " display=" << displayW << "x" << displayH;
    } else {
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] No touch device found — touch input disabled";
    }
```

**Step 2: Add include**

Add at the top of `AndroidAutoPlugin.cpp` (after the existing includes):

```cpp
#include "core/InputDeviceScanner.hpp"
#include <boost/log/trivial.hpp>
```

**Step 3: Build and run tests**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake --build . -j$(nproc) && ctest --output-on-failure
```

Expected: All tests pass. No functional change on dev VM (no touch device found = disabled).

**Step 4: Commit**

```bash
git add src/plugins/android_auto/AndroidAutoPlugin.cpp
git commit -m "feat: auto-detect touch device via INPUT_PROP_DIRECT scan"
```

---

## Task 4: Wire wifi.interface into BluetoothDiscoveryService

**Files:**
- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp:144-166,244-254`

**Step 1: Add wifiInterface member to header**

In `BluetoothDiscoveryService.hpp`, add a member and modify the constructor:

Change the constructor signature from:
```cpp
    explicit BluetoothDiscoveryService(
        std::shared_ptr<oap::Configuration> config,
        QObject* parent = nullptr);
```

To:
```cpp
    explicit BluetoothDiscoveryService(
        std::shared_ptr<oap::Configuration> config,
        const QString& wifiInterface = QStringLiteral("wlan0"),
        QObject* parent = nullptr);
```

Add member in the private section:
```cpp
    QString wifiInterface_;
```

**Step 2: Update constructor and hardcoded references in .cpp**

In `BluetoothDiscoveryService.cpp`, update the constructor (around line 31):

```cpp
BluetoothDiscoveryService::BluetoothDiscoveryService(
    std::shared_ptr<oap::Configuration> config,
    const QString& wifiInterface,
    QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , wifiInterface_(wifiInterface)
    , rfcommServer_(std::make_unique<QBluetoothServer>(
          QBluetoothServiceInfo::RfcommProtocol, this))
```

Replace line 145 (`getLocalIP(QStringLiteral("wlan0"))`) with:
```cpp
    std::string localIp = getLocalIP(wifiInterface_);
```

Replace line 246 (`iface.name() == "wlan0"`) with:
```cpp
        if (iface.name() == wifiInterface_) {
```

**Step 3: Update caller to pass wifi interface from config**

Find where `BluetoothDiscoveryService` is constructed. Search for its creation:

```bash
grep -rn "BluetoothDiscoveryService(" src/ --include="*.cpp"
```

This will be in `AndroidAutoService.cpp`. Update the construction to read `wifi.interface` from config and pass it through. If the constructor currently takes just `(config, parent)`, add the interface string:

```cpp
QString wifiIface = QStringLiteral("wlan0");  // default
// Read from yamlConfig if available
if (yamlConfig_) {
    QString configIface = QString::fromStdString(
        yamlConfig_->getString("wifi.interface", "wlan0"));
    if (!configIface.isEmpty())
        wifiIface = configIface;
}
btDiscovery_ = new BluetoothDiscoveryService(config_, wifiIface, this);
```

**Step 4: Build and run tests**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake --build . -j$(nproc) && ctest --output-on-failure
```

Expected: All tests pass. Default behavior unchanged (falls back to "wlan0").

**Step 5: Commit**

```bash
git add src/core/aa/BluetoothDiscoveryService.hpp src/core/aa/BluetoothDiscoveryService.cpp src/core/aa/AndroidAutoService.cpp
git commit -m "feat: read wifi.interface from config for BT discovery"
```

---

## Task 5: Install script — wireless interface detection + network config

**Files:**
- Modify: `install.sh:118-148` (setup_hardware), add new function

**Step 1: Add wireless interface detection to setup_hardware()**

Replace the WiFi AP section in `setup_hardware()` (lines 133-138) with:

```bash
    # WiFi AP — detect wireless interfaces
    info "Detecting wireless interfaces..."
    WIFI_INTERFACES=()
    for iface_path in /sys/class/net/*/wireless; do
        if [[ -e "$iface_path" ]]; then
            WIFI_INTERFACES+=("$(basename "$(dirname "$iface_path")")")
        fi
    done

    if [[ ${#WIFI_INTERFACES[@]} -eq 0 ]]; then
        warn "No wireless interfaces found!"
        warn "WiFi AP will not be configured. You can set this up manually later."
        WIFI_IFACE=""
    elif [[ ${#WIFI_INTERFACES[@]} -eq 1 ]]; then
        WIFI_IFACE="${WIFI_INTERFACES[0]}"
        info "Found wireless interface: $WIFI_IFACE"
    else
        echo "Multiple wireless interfaces found:"
        for i in "${!WIFI_INTERFACES[@]}"; do
            echo "  $((i+1)). ${WIFI_INTERFACES[$i]}"
        done
        read -p "Select interface for WiFi AP [1]: " WIFI_CHOICE
        WIFI_CHOICE=${WIFI_CHOICE:-1}
        WIFI_IFACE="${WIFI_INTERFACES[$((WIFI_CHOICE-1))]}"
    fi

    if [[ -n "$WIFI_IFACE" ]]; then
        read -p "WiFi hotspot SSID [OpenAutoProdigy]: " WIFI_SSID
        WIFI_SSID=${WIFI_SSID:-OpenAutoProdigy}

        read -p "WiFi hotspot password [openauto123]: " WIFI_PASS
        WIFI_PASS=${WIFI_PASS:-openauto123}

        read -p "AP static IP [10.0.0.1]: " AP_IP
        AP_IP=${AP_IP:-10.0.0.1}

        read -p "Country code (for 5GHz) [US]: " COUNTRY_CODE
        COUNTRY_CODE=${COUNTRY_CODE:-US}
    fi
```

**Step 2: Add configure_network() function**

Add a new function after `setup_hardware()`:

```bash
# ────────────────────────────────────────────────────
# Step 5b: Configure WiFi AP networking
# ────────────────────────────────────────────────────
configure_network() {
    if [[ -z "$WIFI_IFACE" ]]; then
        warn "Skipping network configuration (no wireless interface)"
        return
    fi

    info "Configuring WiFi AP on $WIFI_IFACE..."

    # systemd-networkd config for static IP + built-in DHCP server
    sudo mkdir -p /etc/systemd/network
    sudo tee /etc/systemd/network/10-openauto-ap.network > /dev/null << NETCFG
[Match]
Name=$WIFI_IFACE

[Network]
Address=${AP_IP}/24
DHCPServer=yes

[DHCPServer]
PoolOffset=10
PoolSize=40
EmitDNS=no
NETCFG

    # hostapd config
    sudo tee /etc/hostapd/hostapd.conf > /dev/null << HOSTAPD
interface=$WIFI_IFACE
driver=nl80211
ssid=$WIFI_SSID
hw_mode=a
channel=36
ieee80211n=1
ieee80211ac=1
wmm_enabled=1
country_code=$COUNTRY_CODE
ieee80211d=1
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=$WIFI_PASS
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
HOSTAPD

    # Point hostapd at our config
    if [[ -f /etc/default/hostapd ]]; then
        sudo sed -i 's|^#\?DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
    fi

    # Enable services
    sudo systemctl unmask hostapd 2>/dev/null || true
    sudo systemctl enable hostapd
    sudo systemctl enable systemd-networkd

    ok "WiFi AP configured: SSID=$WIFI_SSID on $WIFI_IFACE ($AP_IP)"
}
```

**Step 3: Update generate_config() to include new settings**

In `generate_config()`, update the config.yaml template to include:

```yaml
wifi:
  interface: "$WIFI_IFACE"
  ssid: "$WIFI_SSID"
  password: "$WIFI_PASS"

android_auto:
  tcp_port: $TCP_PORT
  video_fps: $VIDEO_FPS
  video_resolution: "480p"

display:
  width: 1024
  height: 600
  brightness: 80
  night_mode: "auto"

touch:
  device: ""
```

Note: `touch.device` is empty — auto-detect is the default.

**Step 4: Add configure_network to main()**

In the `main()` function, add `configure_network` between `generate_config` and `create_service`:

```bash
main() {
    print_header
    check_system
    install_dependencies
    setup_hardware
    build_project
    generate_config
    configure_network
    create_service
    create_web_service
    run_diagnostics
}
```

**Step 5: Remove dnsmasq from dependencies**

In `install_dependencies()`, remove `dnsmasq` from the `PACKAGES` array (line ~102). systemd-networkd's built-in DHCP server replaces it.

**Step 6: Commit**

```bash
git add install.sh
git commit -m "feat: install script auto-detects wireless interfaces, configures AP networking"
```

---

## Task 6: Install script — systemd service cleanup + opt-in auto-start

**Files:**
- Modify: `install.sh` — `create_service()` function and `setup_hardware()`

**Step 1: Add auto-start prompt to setup_hardware()**

At the end of `setup_hardware()`, add:

```bash
    # Auto-start
    echo
    read -p "Start OpenAuto Prodigy automatically on boot? [y/N] " -n 1 -r
    echo
    AUTOSTART=false
    [[ $REPLY =~ ^[Yy]$ ]] && AUTOSTART=true
```

**Step 2: Clean up create_service()**

Replace the current `create_service()` function with:

```bash
create_service() {
    info "Creating systemd service..."

    sudo tee /etc/systemd/system/${SERVICE_NAME}.service > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy
After=graphical.target

[Service]
Type=simple
User=$USER
Environment=XDG_RUNTIME_DIR=/run/user/$(id -u)
Environment=WAYLAND_DISPLAY=wayland-0
Environment=QT_QPA_PLATFORM=wayland
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/build/src/openauto-prodigy
Restart=on-failure
RestartSec=3

[Install]
WantedBy=graphical.target
SERVICE

    sudo systemctl daemon-reload

    if [[ "$AUTOSTART" == "true" ]]; then
        sudo systemctl enable ${SERVICE_NAME}
        ok "Service created and enabled (auto-start on boot)"
    else
        ok "Service created (manual start: sudo systemctl start ${SERVICE_NAME})"
    fi
}
```

Changes from original:
- Removed `Wants=pipewire.service bluetooth.service` (they start independently)
- Removed `After=pipewire.service bluetooth.service` (only need graphical.target)
- Added `WorkingDirectory`
- `systemctl enable` is conditional on user choice

**Step 3: Commit**

```bash
git add install.sh
git commit -m "feat: clean up systemd service, make auto-start opt-in"
```

---

## Task 7: Update wireless-setup.md + diagnostics

**Files:**
- Modify: `docs/wireless-setup.md`
- Modify: `install.sh` — `run_diagnostics()` function

**Step 1: Update wireless-setup.md**

Replace the "Configure static IP on wlan0" section (lines 57-81) to document the new systemd-networkd approach used by the install script, and note that the install script handles this automatically. Keep the manual instructions as a fallback reference.

Add a note about multi-interface setups:

```markdown
### Multiple wireless interfaces

If your Pi has both built-in WiFi and a USB wireless adapter, the install script
will detect all wireless interfaces and let you choose which one runs the AP.
Set `wifi.interface` in `~/.openauto/config.yaml` to match your choice.
```

**Step 2: Update diagnostics to show configured interface**

In `run_diagnostics()`, update the WiFi AP section to read the config:

```bash
    # WiFi AP
    echo
    if [[ -n "$WIFI_IFACE" ]]; then
        if systemctl is-active hostapd &>/dev/null; then
            ok "WiFi AP: running on $WIFI_IFACE (SSID: $WIFI_SSID)"
        else
            warn "WiFi AP: not running on $WIFI_IFACE"
        fi
    else
        info "WiFi AP: not configured"
    fi
```

**Step 3: Commit**

```bash
git add docs/wireless-setup.md install.sh
git commit -m "docs: update wireless setup for auto-configured networking"
```

---

## Task 8: Build, test, deploy to Pi

**Step 1: Full build + test locally**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure
```

Expected: All tests pass (10 tests now — 9 existing + 1 new InputDeviceScanner test).

**Step 2: Deploy to Pi**

```bash
rsync -av --exclude build/ --exclude .git/ \
    /home/matt/claude/personal/openautopro/openauto-prodigy/src/ \
    matt@192.168.1.149:~/openauto-prodigy/src/
rsync -av --exclude build/ --exclude .git/ \
    /home/matt/claude/personal/openautopro/openauto-prodigy/tests/ \
    matt@192.168.1.149:~/openauto-prodigy/tests/
rsync -av \
    /home/matt/claude/personal/openautopro/openauto-prodigy/install.sh \
    matt@192.168.1.149:~/openauto-prodigy/
rsync -av \
    /home/matt/claude/personal/openautopro/openauto-prodigy/docs/ \
    matt@192.168.1.149:~/openauto-prodigy/docs/
```

**Step 3: Build on Pi**

```bash
ssh matt@192.168.1.149 "cd ~/openauto-prodigy/build && cmake .. && cmake --build . -j3"
```

**Step 4: Test on Pi**

```bash
ssh matt@192.168.1.149 "cd ~/openauto-prodigy/build && ctest --output-on-failure"
```

The `test_input_device_scanner` test should find the DFRobot touchscreen on the Pi.

**Step 5: Launch and verify touch auto-detect**

```bash
ssh matt@192.168.1.149 'pkill -f openauto; sleep 1; cd ~/openauto-prodigy && nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
sleep 3
ssh matt@192.168.1.149 "grep -E 'Auto-detected|Touch|touch' /tmp/oap.log"
```

Expected: Log shows "Auto-detected touch device: /dev/input/eventN (DFRobot USB Multi Touch)"

**Step 6: Commit final state**

```bash
git add -A
git commit -m "feat: infrastructure improvements — touch auto-detect, network config, boot service"
```

(Only if there are uncommitted changes from deployment testing.)
