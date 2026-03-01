#pragma once

#include "IDisplayService.hpp"
#include <QObject>
#include <QSize>
#include <QString>

namespace oap {

/// Concrete display service with 3-tier brightness backend:
/// 1. sysfs backlight (Pi / laptop with backlight device)
/// 2. ddcutil DDC/CI (external monitors)
/// 3. Software overlay fallback (QML dim overlay)
///
/// Software overlay exposes dimOverlayOpacity for QML binding.
/// Brightness is clamped to 5-100 to prevent black screen.
class DisplayService : public QObject, public IDisplayService {
    Q_OBJECT
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal dimOverlayOpacity READ dimOverlayOpacity NOTIFY brightnessChanged)

public:
    explicit DisplayService(QObject* parent = nullptr);

    // IDisplayService
    QSize screenSize() const override;
    int brightness() const override;
    void setBrightness(int value) override;
    QString orientation() const override;

    /// Overlay opacity for software dimming (0.0 = invisible, max 0.9).
    qreal dimOverlayOpacity() const;

    /// Human-readable name of the active backend (for logging/debug).
    QString backendName() const;

signals:
    void brightnessChanged();

private:
    enum class Backend { SysfsBacklight, DdcUtil, SoftwareOverlay };
    Backend detectBackend();
    void applySysfs(int value);
    void applyDdcutil(int value);

    Backend backend_;
    int brightness_ = 80;
    QString sysfsPath_;
    int sysfsMax_ = 255;
};

} // namespace oap
