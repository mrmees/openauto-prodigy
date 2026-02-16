#include "core/Configuration.hpp"
#include <QSettings>

namespace oap {

// --- Enum conversion helpers ---

static QString screenTypeToString(ScreenType t) {
    switch (t) {
    case ScreenType::Wide:     return QStringLiteral("WIDE");
    case ScreenType::Standard:
    default:                   return QStringLiteral("STANDARD");
    }
}

static ScreenType screenTypeFromString(const QString& s) {
    if (s.compare(QStringLiteral("WIDE"), Qt::CaseInsensitive) == 0)
        return ScreenType::Wide;
    return ScreenType::Standard;
}

static QString handednessToString(Handedness h) {
    switch (h) {
    case Handedness::RHD: return QStringLiteral("RHD");
    case Handedness::LHD:
    default:              return QStringLiteral("LHD");
    }
}

static Handedness handednessFromString(const QString& s) {
    if (s.compare(QStringLiteral("RHD"), Qt::CaseInsensitive) == 0)
        return Handedness::RHD;
    return Handedness::LHD;
}

static QString timeFormatToString(TimeFormat f) {
    switch (f) {
    case TimeFormat::Format24H: return QStringLiteral("FORMAT_24H");
    case TimeFormat::Format12H:
    default:                    return QStringLiteral("FORMAT_12H");
    }
}

static TimeFormat timeFormatFromString(const QString& s) {
    if (s.compare(QStringLiteral("FORMAT_24H"), Qt::CaseInsensitive) == 0)
        return TimeFormat::Format24H;
    return TimeFormat::Format12H;
}

static QString btAdapterTypeToString(BluetoothAdapterType t) {
    switch (t) {
    case BluetoothAdapterType::None:   return QStringLiteral("NONE");
    case BluetoothAdapterType::Remote: return QStringLiteral("REMOTE");
    case BluetoothAdapterType::Local:
    default:                           return QStringLiteral("LOCAL");
    }
}

static BluetoothAdapterType btAdapterTypeFromString(const QString& s) {
    if (s.compare(QStringLiteral("NONE"), Qt::CaseInsensitive) == 0)
        return BluetoothAdapterType::None;
    if (s.compare(QStringLiteral("REMOTE"), Qt::CaseInsensitive) == 0)
        return BluetoothAdapterType::Remote;
    return BluetoothAdapterType::Local;
}

// --- Configuration ---

Configuration::Configuration() {
    // Day colors (defaults matching openauto_system.ini)
    m_dayColors.backgroundColor          = QColor(QStringLiteral("#1a1a2e"));
    m_dayColors.highlightColor           = QColor(QStringLiteral("#e94560"));
    m_dayColors.controlBackgroundColor   = QColor(QStringLiteral("#16213e"));
    m_dayColors.controlForegroundColor   = QColor(QStringLiteral("#0f3460"));
    m_dayColors.normalFontColor          = QColor(QStringLiteral("#eaeaea"));
    m_dayColors.specialFontColor         = QColor(QStringLiteral("#e94560"));
    m_dayColors.descriptionFontColor     = QColor(QStringLiteral("#a0a0a0"));
    m_dayColors.barBackgroundColor       = QColor(QStringLiteral("#16213e"));
    m_dayColors.controlBoxBackgroundColor = QColor(QStringLiteral("#0f3460"));
    m_dayColors.gaugeIndicatorColor      = QColor(QStringLiteral("#e94560"));
    m_dayColors.iconColor                = QColor(QStringLiteral("#eaeaea"));
    m_dayColors.sideWidgetBackgroundColor = QColor(QStringLiteral("#16213e"));
    m_dayColors.barShadowColor           = QColor(QStringLiteral("#0a0a1a"));

    // Night colors (defaults matching openauto_system.ini)
    m_nightColors.backgroundColor          = QColor(QStringLiteral("#0a0a1a"));
    m_nightColors.highlightColor           = QColor(QStringLiteral("#c73650"));
    m_nightColors.controlBackgroundColor   = QColor(QStringLiteral("#0d1829"));
    m_nightColors.controlForegroundColor   = QColor(QStringLiteral("#091833"));
    m_nightColors.normalFontColor          = QColor(QStringLiteral("#c0c0c0"));
    m_nightColors.specialFontColor         = QColor(QStringLiteral("#c73650"));
    m_nightColors.descriptionFontColor     = QColor(QStringLiteral("#808080"));
    m_nightColors.barBackgroundColor       = QColor(QStringLiteral("#0d1829"));
    m_nightColors.controlBoxBackgroundColor = QColor(QStringLiteral("#091833"));
    m_nightColors.gaugeIndicatorColor      = QColor(QStringLiteral("#c73650"));
    m_nightColors.iconColor                = QColor(QStringLiteral("#c0c0c0"));
    m_nightColors.sideWidgetBackgroundColor = QColor(QStringLiteral("#0d1829"));
    m_nightColors.barShadowColor           = QColor(QStringLiteral("#050510"));
}

void Configuration::load(const QString& filePath) {
    QSettings settings(filePath, QSettings::IniFormat);

    // [AndroidAuto]
    settings.beginGroup(QStringLiteral("AndroidAuto"));
    m_dayNightModeController = settings.value(QStringLiteral("day_night_mode_controller"), m_dayNightModeController).toBool();
    m_showClockInAndroidAuto = settings.value(QStringLiteral("show_clock_in_android_auto"), m_showClockInAndroidAuto).toBool();
    m_showTopBar             = settings.value(QStringLiteral("show_top_bar"), m_showTopBar).toBool();
    settings.endGroup();

    // [Display]
    settings.beginGroup(QStringLiteral("Display"));
    m_screenType = screenTypeFromString(settings.value(QStringLiteral("screen_type"), screenTypeToString(m_screenType)).toString());
    m_handedness = handednessFromString(settings.value(QStringLiteral("handedness_of_traffic"), handednessToString(m_handedness)).toString());
    m_screenDpi  = settings.value(QStringLiteral("screen_dpi"), m_screenDpi).toInt();
    settings.endGroup();

    // [Colors] and [Colors_Night]
    loadColorSet(settings, QStringLiteral("Colors"), m_dayColors);
    loadColorSet(settings, QStringLiteral("Colors_Night"), m_nightColors);

    // [Audio]
    settings.beginGroup(QStringLiteral("Audio"));
    m_volumeStep = settings.value(QStringLiteral("volume_step"), m_volumeStep).toInt();
    settings.endGroup();

    // [Bluetooth]
    settings.beginGroup(QStringLiteral("Bluetooth"));
    m_btAdapterType = btAdapterTypeFromString(settings.value(QStringLiteral("adapter_type"), btAdapterTypeToString(m_btAdapterType)).toString());
    settings.endGroup();

    // [System]
    settings.beginGroup(QStringLiteral("System"));
    m_language   = settings.value(QStringLiteral("language"), m_language).toString();
    m_timeFormat = timeFormatFromString(settings.value(QStringLiteral("time_format"), timeFormatToString(m_timeFormat)).toString());
    settings.endGroup();
}

void Configuration::save(const QString& filePath) const {
    QSettings settings(filePath, QSettings::IniFormat);

    // [AndroidAuto]
    settings.beginGroup(QStringLiteral("AndroidAuto"));
    settings.setValue(QStringLiteral("day_night_mode_controller"), m_dayNightModeController);
    settings.setValue(QStringLiteral("show_clock_in_android_auto"), m_showClockInAndroidAuto);
    settings.setValue(QStringLiteral("show_top_bar"), m_showTopBar);
    settings.endGroup();

    // [Display]
    settings.beginGroup(QStringLiteral("Display"));
    settings.setValue(QStringLiteral("screen_type"), screenTypeToString(m_screenType));
    settings.setValue(QStringLiteral("handedness_of_traffic"), handednessToString(m_handedness));
    settings.setValue(QStringLiteral("screen_dpi"), m_screenDpi);
    settings.endGroup();

    // [Colors] and [Colors_Night]
    saveColorSet(settings, QStringLiteral("Colors"), m_dayColors);
    saveColorSet(settings, QStringLiteral("Colors_Night"), m_nightColors);

    // [Audio]
    settings.beginGroup(QStringLiteral("Audio"));
    settings.setValue(QStringLiteral("volume_step"), m_volumeStep);
    settings.endGroup();

    // [Bluetooth]
    settings.beginGroup(QStringLiteral("Bluetooth"));
    settings.setValue(QStringLiteral("adapter_type"), btAdapterTypeToString(m_btAdapterType));
    settings.endGroup();

    // [System]
    settings.beginGroup(QStringLiteral("System"));
    settings.setValue(QStringLiteral("language"), m_language);
    settings.setValue(QStringLiteral("time_format"), timeFormatToString(m_timeFormat));
    settings.endGroup();

    settings.sync();
}

void Configuration::loadColorSet(QSettings& settings, const QString& group, ColorSet& cs) {
    settings.beginGroup(group);
    auto readColor = [&](const QString& key, QColor& target) {
        const QString val = settings.value(key).toString();
        if (!val.isEmpty()) {
            target = QColor(val);
        }
    };
    readColor(QStringLiteral("background_color"),           cs.backgroundColor);
    readColor(QStringLiteral("highlight_color"),            cs.highlightColor);
    readColor(QStringLiteral("control_background_color"),   cs.controlBackgroundColor);
    readColor(QStringLiteral("control_foreground_color"),   cs.controlForegroundColor);
    readColor(QStringLiteral("normal_font_color"),          cs.normalFontColor);
    readColor(QStringLiteral("special_font_color"),         cs.specialFontColor);
    readColor(QStringLiteral("description_font_color"),     cs.descriptionFontColor);
    readColor(QStringLiteral("bar_background_color"),       cs.barBackgroundColor);
    readColor(QStringLiteral("control_box_background_color"), cs.controlBoxBackgroundColor);
    readColor(QStringLiteral("gauge_indicator_color"),      cs.gaugeIndicatorColor);
    readColor(QStringLiteral("icon_color"),                 cs.iconColor);
    readColor(QStringLiteral("side_widget_background_color"), cs.sideWidgetBackgroundColor);
    readColor(QStringLiteral("bar_shadow_color"),           cs.barShadowColor);
    settings.endGroup();
}

void Configuration::saveColorSet(QSettings& settings, const QString& group, const ColorSet& cs) const {
    settings.beginGroup(group);
    settings.setValue(QStringLiteral("background_color"),           cs.backgroundColor.name());
    settings.setValue(QStringLiteral("highlight_color"),            cs.highlightColor.name());
    settings.setValue(QStringLiteral("control_background_color"),   cs.controlBackgroundColor.name());
    settings.setValue(QStringLiteral("control_foreground_color"),   cs.controlForegroundColor.name());
    settings.setValue(QStringLiteral("normal_font_color"),          cs.normalFontColor.name());
    settings.setValue(QStringLiteral("special_font_color"),         cs.specialFontColor.name());
    settings.setValue(QStringLiteral("description_font_color"),     cs.descriptionFontColor.name());
    settings.setValue(QStringLiteral("bar_background_color"),       cs.barBackgroundColor.name());
    settings.setValue(QStringLiteral("control_box_background_color"), cs.controlBoxBackgroundColor.name());
    settings.setValue(QStringLiteral("gauge_indicator_color"),      cs.gaugeIndicatorColor.name());
    settings.setValue(QStringLiteral("icon_color"),                 cs.iconColor.name());
    settings.setValue(QStringLiteral("side_widget_background_color"), cs.sideWidgetBackgroundColor.name());
    settings.setValue(QStringLiteral("bar_shadow_color"),           cs.barShadowColor.name());
    settings.endGroup();
}

// --- Color getters ---

const Configuration::ColorSet& Configuration::colorSet(ThemeMode mode) const {
    return (mode == ThemeMode::Night) ? m_nightColors : m_dayColors;
}

Configuration::ColorSet& Configuration::colorSet(ThemeMode mode) {
    return (mode == ThemeMode::Night) ? m_nightColors : m_dayColors;
}

QColor Configuration::backgroundColor(ThemeMode mode) const          { return colorSet(mode).backgroundColor; }
QColor Configuration::highlightColor(ThemeMode mode) const           { return colorSet(mode).highlightColor; }
QColor Configuration::controlBackgroundColor(ThemeMode mode) const   { return colorSet(mode).controlBackgroundColor; }
QColor Configuration::controlForegroundColor(ThemeMode mode) const   { return colorSet(mode).controlForegroundColor; }
QColor Configuration::normalFontColor(ThemeMode mode) const          { return colorSet(mode).normalFontColor; }
QColor Configuration::specialFontColor(ThemeMode mode) const         { return colorSet(mode).specialFontColor; }
QColor Configuration::descriptionFontColor(ThemeMode mode) const     { return colorSet(mode).descriptionFontColor; }
QColor Configuration::barBackgroundColor(ThemeMode mode) const       { return colorSet(mode).barBackgroundColor; }
QColor Configuration::controlBoxBackgroundColor(ThemeMode mode) const { return colorSet(mode).controlBoxBackgroundColor; }
QColor Configuration::gaugeIndicatorColor(ThemeMode mode) const      { return colorSet(mode).gaugeIndicatorColor; }
QColor Configuration::iconColor(ThemeMode mode) const                { return colorSet(mode).iconColor; }
QColor Configuration::sideWidgetBackgroundColor(ThemeMode mode) const { return colorSet(mode).sideWidgetBackgroundColor; }
QColor Configuration::barShadowColor(ThemeMode mode) const           { return colorSet(mode).barShadowColor; }

// --- Color setters ---

void Configuration::setBackgroundColor(ThemeMode mode, const QColor& c)          { colorSet(mode).backgroundColor = c; }
void Configuration::setHighlightColor(ThemeMode mode, const QColor& c)           { colorSet(mode).highlightColor = c; }
void Configuration::setControlBackgroundColor(ThemeMode mode, const QColor& c)   { colorSet(mode).controlBackgroundColor = c; }
void Configuration::setControlForegroundColor(ThemeMode mode, const QColor& c)   { colorSet(mode).controlForegroundColor = c; }
void Configuration::setNormalFontColor(ThemeMode mode, const QColor& c)          { colorSet(mode).normalFontColor = c; }
void Configuration::setSpecialFontColor(ThemeMode mode, const QColor& c)         { colorSet(mode).specialFontColor = c; }
void Configuration::setDescriptionFontColor(ThemeMode mode, const QColor& c)     { colorSet(mode).descriptionFontColor = c; }
void Configuration::setBarBackgroundColor(ThemeMode mode, const QColor& c)       { colorSet(mode).barBackgroundColor = c; }
void Configuration::setControlBoxBackgroundColor(ThemeMode mode, const QColor& c) { colorSet(mode).controlBoxBackgroundColor = c; }
void Configuration::setGaugeIndicatorColor(ThemeMode mode, const QColor& c)      { colorSet(mode).gaugeIndicatorColor = c; }
void Configuration::setIconColor(ThemeMode mode, const QColor& c)                { colorSet(mode).iconColor = c; }
void Configuration::setSideWidgetBackgroundColor(ThemeMode mode, const QColor& c) { colorSet(mode).sideWidgetBackgroundColor = c; }
void Configuration::setBarShadowColor(ThemeMode mode, const QColor& c)           { colorSet(mode).barShadowColor = c; }

} // namespace oap
