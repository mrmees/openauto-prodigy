# QR Code Pairing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Display a scannable QR code on the head unit screen for companion app pairing, replacing manual PIN entry.

**Architecture:** Vendor the qrcodegen single-header C++ library. When the user generates a pairing PIN, also encode an `openauto://pair?pin=...&host=...&port=...` URI into a QR code rendered as SVG, exposed to QML as a base64 data URI. PIN text remains as fallback.

**Tech Stack:** qrcodegen (MIT, header-only C++), Qt 6 Q_PROPERTY, QML Image with data URI

---

### Task 1: Vendor qrcodegen library

**Files:**
- Create: `libs/qrcodegen/qrcodegen.hpp`
- Modify: `src/CMakeLists.txt:245-251`

**Step 1: Download qrcodegen header**

Download the single C++ header from the qrcodegen project (nayuki/QR-Code-generator on GitHub). The file is `cpp/qrcodegen.hpp`. It's header-only but also has a `.cpp` file — we need both.

Actually, qrcodegen is header + source (not header-only). We need:
- `libs/qrcodegen/qrcodegen.hpp`
- `libs/qrcodegen/qrcodegen.cpp`

Download from: https://raw.githubusercontent.com/nayuki/QR-Code-generator/master/cpp/qrcodegen.hpp
Download from: https://raw.githubusercontent.com/nayuki/QR-Code-generator/master/cpp/qrcodegen.cpp

**Step 2: Add to CMakeLists.txt include path**

In `src/CMakeLists.txt`, add `${CMAKE_SOURCE_DIR}/libs/qrcodegen` to the `target_include_directories` block at line 245.

Also add `${CMAKE_SOURCE_DIR}/libs/qrcodegen/qrcodegen.cpp` to the source file list for `openauto-prodigy`.

**Step 3: Verify build**

Run: `cmake --build build -j$(nproc)`
Expected: Clean build, no new warnings from qrcodegen.

**Step 4: Commit**

```bash
git add libs/qrcodegen/ src/CMakeLists.txt
git commit -m "vendor: add qrcodegen library (MIT, nayuki/QR-Code-generator)"
```

---

### Task 2: Add QR generation to CompanionListenerService

**Files:**
- Modify: `src/core/services/CompanionListenerService.hpp`
- Modify: `src/core/services/CompanionListenerService.cpp`

**Step 1: Add Q_PROPERTY and method declarations to header**

In `CompanionListenerService.hpp`:

Add Q_PROPERTY after line 22:
```cpp
Q_PROPERTY(QString qrCodeDataUri READ qrCodeDataUri NOTIFY qrCodeChanged)
```

Add public getter after line 49:
```cpp
QString qrCodeDataUri() const;
```

Add signal after line 56:
```cpp
void qrCodeChanged();
```

Add private method and member after line 92:
```cpp
QString generateQrSvg(const QString& payload);

QString qrCodeDataUri_;
int listenPort_ = 9876;
```

Add to constructor or `start()`: store port in `listenPort_`.

**Step 2: Implement QR generation in .cpp**

Add `#include <qrcodegen.hpp>` at top of CompanionListenerService.cpp.

Implement `generateQrSvg()`:
```cpp
QString CompanionListenerService::generateQrSvg(const QString& payload)
{
    using namespace qrcodegen;
    QrCode qr = QrCode::encodeText(payload.toUtf8().constData(), QrCode::Ecc::MEDIUM);

    int size = qr.getSize();
    int border = 2;
    int total = size + border * 2;
    int scale = 4;  // pixels per module

    QString svg;
    svg += QString("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %1 %1'>").arg(total * scale);
    svg += QString("<rect width='%1' height='%1' fill='white'/>").arg(total * scale);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (qr.getModule(x, y)) {
                svg += QString("<rect x='%1' y='%2' width='%3' height='%3' fill='black'/>")
                    .arg((x + border) * scale)
                    .arg((y + border) * scale)
                    .arg(scale);
            }
        }
    }
    svg += "</svg>";
    return svg;
}
```

Implement `qrCodeDataUri()` getter:
```cpp
QString CompanionListenerService::qrCodeDataUri() const
{
    return qrCodeDataUri_;
}
```

**Step 3: Update generatePairingPin() to also generate QR**

After the existing PIN generation logic (line 97), before the return:

```cpp
// Build QR payload with connection info
QString payload = QString("openauto://pair?pin=%1&host=10.0.0.1&port=%2")
    .arg(pinStr)
    .arg(listenPort_);

QString svg = generateQrSvg(payload);
qrCodeDataUri_ = "data:image/svg+xml;base64," + QString::fromLatin1(svg.toUtf8().toBase64());
emit qrCodeChanged();
```

**Step 4: Store port in start()**

In `start()` method, add `listenPort_ = port;` before the server setup.

**Step 5: Build and verify**

Run: `cmake --build build -j$(nproc)`
Expected: Clean build.

**Step 6: Commit**

```bash
git add src/core/services/CompanionListenerService.hpp src/core/services/CompanionListenerService.cpp
git commit -m "feat: generate QR code data URI on pairing PIN creation"
```

---

### Task 3: Display QR code in CompanionSettings QML

**Files:**
- Modify: `qml/applications/settings/CompanionSettings.qml`

**Step 1: Add QR code Image between the PIN button and PIN text**

After the pairing button item (line 187) and before the PIN display text (line 190), add:

```qml
// QR code (visible after generation)
Image {
    id: qrCode
    visible: root.hasService && CompanionService.qrCodeDataUri !== ""
    Layout.alignment: Qt.AlignHCenter
    Layout.preferredWidth: Math.round(200 * UiMetrics.scale)
    Layout.preferredHeight: Math.round(200 * UiMetrics.scale)
    source: root.hasService ? CompanionService.qrCodeDataUri : ""
    fillMode: Image.PreserveAspectFit
    smooth: false  // crisp pixel edges for QR modules
}
```

**Step 2: Update PIN hint text**

Change the hint text from:
```
"Enter this PIN in the companion app on your phone"
```
to:
```
"Scan the QR code, or enter this PIN manually"
```

**Step 3: Update button text**

Change button text from `"Generate Pairing PIN"` to `"Generate Pairing Code"` (since it now generates both).

**Step 4: Build and verify**

Run: `cmake --build build -j$(nproc)`
Expected: Clean build.

**Step 5: Commit**

```bash
git add qml/applications/settings/CompanionSettings.qml
git commit -m "feat: display QR code in companion settings UI"
```

---

### Task 4: Cross-compile, deploy, and test

**Step 1: Cross-compile for Pi**

```bash
docker run --rm -u "$(id -u):$(id -g)" -v "$(pwd):/src:rw" -w /src/build-pi openauto-cross-pi4 cmake --build . -j$(nproc)
```

**Step 2: Deploy to Pi**

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Step 3: Test on Pi**

1. Launch app on Pi
2. Navigate to Settings > Companion
3. Press "Generate Pairing Code"
4. Verify: QR code appears above the PIN text
5. Verify: QR code is scannable (use any QR scanner app)
6. Verify: Scanned content is `openauto://pair?pin=XXXXXX&host=10.0.0.1&port=9876`
7. Verify: PIN text still displayed below QR as fallback

**Step 4: Commit any fixes, then final commit**

```bash
git commit -m "feat: QR code pairing for companion app"
```
