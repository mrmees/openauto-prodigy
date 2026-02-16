#pragma once

#include <QString>
#include <QColor>

namespace oap {

enum class ScreenType {
    Standard,
    Wide
};

enum class Handedness {
    LHD,
    RHD
};

enum class TimeFormat {
    Format12H,
    Format24H
};

enum class ThemeMode {
    Day,
    Night
};

enum class BluetoothAdapterType {
    None,
    Local,
    Remote
};

enum class DayNightController {
    Manual,
    Sensor,
    Clock,
    Gpio
};

class Configuration {
public:
    Configuration();

    void load(const QString& filePath);
    void save(const QString& filePath) const;

    // --- AndroidAuto ---
    bool dayNightModeController() const { return m_dayNightModeController; }
    void setDayNightModeController(bool v) { m_dayNightModeController = v; }

    bool showClockInAndroidAuto() const { return m_showClockInAndroidAuto; }
    void setShowClockInAndroidAuto(bool v) { m_showClockInAndroidAuto = v; }

    bool showTopBar() const { return m_showTopBar; }
    void setShowTopBar(bool v) { m_showTopBar = v; }

    // --- Display ---
    ScreenType screenType() const { return m_screenType; }
    void setScreenType(ScreenType v) { m_screenType = v; }

    Handedness handednessOfTraffic() const { return m_handedness; }
    void setHandednessOfTraffic(Handedness v) { m_handedness = v; }

    int screenDpi() const { return m_screenDpi; }
    void setScreenDpi(int v) { m_screenDpi = v; }

    // --- Audio ---
    int volumeStep() const { return m_volumeStep; }
    void setVolumeStep(int v) { m_volumeStep = v; }

    // --- Bluetooth ---
    BluetoothAdapterType bluetoothAdapterType() const { return m_btAdapterType; }
    void setBluetoothAdapterType(BluetoothAdapterType v) { m_btAdapterType = v; }

    // --- System ---
    QString language() const { return m_language; }
    void setLanguage(const QString& v) { m_language = v; }

    TimeFormat timeFormat() const { return m_timeFormat; }
    void setTimeFormat(TimeFormat v) { m_timeFormat = v; }

    // --- Colors (theme-aware) ---
    QColor backgroundColor(ThemeMode mode) const;
    QColor highlightColor(ThemeMode mode) const;
    QColor controlBackgroundColor(ThemeMode mode) const;
    QColor controlForegroundColor(ThemeMode mode) const;
    QColor normalFontColor(ThemeMode mode) const;
    QColor specialFontColor(ThemeMode mode) const;
    QColor descriptionFontColor(ThemeMode mode) const;
    QColor barBackgroundColor(ThemeMode mode) const;
    QColor controlBoxBackgroundColor(ThemeMode mode) const;
    QColor gaugeIndicatorColor(ThemeMode mode) const;
    QColor iconColor(ThemeMode mode) const;
    QColor sideWidgetBackgroundColor(ThemeMode mode) const;
    QColor barShadowColor(ThemeMode mode) const;

    void setBackgroundColor(ThemeMode mode, const QColor& c);
    void setHighlightColor(ThemeMode mode, const QColor& c);
    void setControlBackgroundColor(ThemeMode mode, const QColor& c);
    void setControlForegroundColor(ThemeMode mode, const QColor& c);
    void setNormalFontColor(ThemeMode mode, const QColor& c);
    void setSpecialFontColor(ThemeMode mode, const QColor& c);
    void setDescriptionFontColor(ThemeMode mode, const QColor& c);
    void setBarBackgroundColor(ThemeMode mode, const QColor& c);
    void setControlBoxBackgroundColor(ThemeMode mode, const QColor& c);
    void setGaugeIndicatorColor(ThemeMode mode, const QColor& c);
    void setIconColor(ThemeMode mode, const QColor& c);
    void setSideWidgetBackgroundColor(ThemeMode mode, const QColor& c);
    void setBarShadowColor(ThemeMode mode, const QColor& c);

private:
    struct ColorSet {
        QColor backgroundColor;
        QColor highlightColor;
        QColor controlBackgroundColor;
        QColor controlForegroundColor;
        QColor normalFontColor;
        QColor specialFontColor;
        QColor descriptionFontColor;
        QColor barBackgroundColor;
        QColor controlBoxBackgroundColor;
        QColor gaugeIndicatorColor;
        QColor iconColor;
        QColor sideWidgetBackgroundColor;
        QColor barShadowColor;
    };

    const ColorSet& colorSet(ThemeMode mode) const;
    ColorSet& colorSet(ThemeMode mode);

    void loadColorSet(class QSettings& settings, const QString& group, ColorSet& cs);
    void saveColorSet(class QSettings& settings, const QString& group, const ColorSet& cs) const;

    // AndroidAuto
    bool m_dayNightModeController = true;
    bool m_showClockInAndroidAuto = true;
    bool m_showTopBar = true;

    // Display
    ScreenType m_screenType = ScreenType::Standard;
    Handedness m_handedness = Handedness::LHD;
    int m_screenDpi = 140;

    // Audio
    int m_volumeStep = 5;

    // Bluetooth
    BluetoothAdapterType m_btAdapterType = BluetoothAdapterType::Local;

    // System
    QString m_language = QStringLiteral("en_US");
    TimeFormat m_timeFormat = TimeFormat::Format12H;

    // Colors
    ColorSet m_dayColors;
    ColorSet m_nightColors;
};

} // namespace oap
