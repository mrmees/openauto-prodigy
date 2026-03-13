#pragma once

#include <QObject>
#include <QString>
#include <cmath>
#include <utility>

namespace oap {

class IConfigService;
class DisplayInfo;

namespace aa {

class AndroidAutoOrchestrator;
class EvdevTouchReader;
class EvdevCoordBridge;

/// Manages platform-adjacent policy for Android Auto that doesn't belong
/// in the plugin itself: touch device detection, EvdevTouchReader lifecycle,
/// EvdevCoordBridge creation, display dimension injection, and navbar
/// thickness calculation.
///
/// AndroidAutoPlugin retains AA protocol lifecycle (orchestrator start/stop,
/// QML context exposure, connection state signals, config change watching).
class AndroidAutoRuntimeBridge : public QObject {
    Q_OBJECT

public:
    explicit AndroidAutoRuntimeBridge(QObject* parent = nullptr);
    ~AndroidAutoRuntimeBridge() override;

    /// Compute DPI-scaled navbar thickness.
    /// Formula: round(56 * (dpi / 160) * globalScale)
    /// Returns 56 (base) if dpi <= 0.
    static int computeNavbarThickness(int dpi, double globalScale);

    /// Resolve AA video coordinate space from config.
    /// Returns {width, height} based on video.resolution setting.
    static std::pair<int, int> resolveAAResolution(IConfigService* configService);

    /// Resolve the touch device path: first from config, then auto-detect.
    QString resolveTouchDevice(IConfigService* configService) const;

    /// Initialize the bridge: create EvdevTouchReader, EvdevCoordBridge,
    /// inject display dimensions and navbar thickness into orchestrator.
    /// Call after AndroidAutoPlugin::initialize() creates the orchestrator.
    void setup(AndroidAutoOrchestrator* orchestrator,
               DisplayInfo* displayInfo,
               IConfigService* configService);

    /// Shutdown: stop touch reader.
    void shutdown();

    // --- Accessors ---
    EvdevTouchReader* touchReader() const { return touchReader_; }
    EvdevCoordBridge* coordBridge() const { return coordBridge_; }
    int displayWidth() const { return displayW_; }
    int displayHeight() const { return displayH_; }
    int navbarThickness() const { return navbarThick_; }

    /// Grab/ungrab the evdev device (delegates to touchReader).
    void grab();
    void ungrab();

signals:
    /// Relayed from EvdevTouchReader::gestureDetected.
    void gestureTriggered();

private:
    EvdevTouchReader* touchReader_ = nullptr;
    EvdevCoordBridge* coordBridge_ = nullptr;
    DisplayInfo* displayInfo_ = nullptr;
    IConfigService* configService_ = nullptr;
    int displayW_ = 1024;
    int displayH_ = 600;
    int navbarThick_ = 56;
};

} // namespace aa
} // namespace oap
