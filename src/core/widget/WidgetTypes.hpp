#pragma once

#include <QFlags>
#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace oap {

enum class WidgetSize {
    Main = 0x1,
    Sub  = 0x2
};
Q_DECLARE_FLAGS(WidgetSizeFlags, WidgetSize)
Q_DECLARE_OPERATORS_FOR_FLAGS(WidgetSizeFlags)

struct WidgetDescriptor {
    QString id;                         // "org.openauto.clock"
    QString displayName;                // "Clock"
    QString iconName;                   // "schedule" (Material Symbols name)
    WidgetSizeFlags supportedSizes;     // Main | Sub
    QUrl qmlComponent;                  // qrc URL to widget QML
    QString pluginId;                   // empty for standalone widgets
    QVariantMap defaultConfig;          // optional per-widget defaults
};

struct WidgetPlacement {
    QString instanceId;     // "clock-main" — unique per placement
    QString widgetId;       // "org.openauto.clock"
    QString pageId;         // "home"
    QString paneId;         // "main", "sub1", "sub2"
    QVariantMap config;     // optional per-instance config

    QString compositeKey() const { return pageId + ":" + paneId; }
};

struct PageDescriptor {
    QString id;
    QString layoutTemplate = QStringLiteral("standard-3pane");
    int order = 0;
    QVariantMap flags;
};

} // namespace oap
