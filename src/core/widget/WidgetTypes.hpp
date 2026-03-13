#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace oap {

struct WidgetDescriptor {
    QString id;                         // "org.openauto.clock"
    QString displayName;                // "Clock"
    QString iconName;                   // Material icon codepoint (e.g. "\ue8b5")
    QUrl qmlComponent;                  // qrc URL to widget QML (empty for stubs)
    QString pluginId;                   // empty for standalone widgets
    QVariantMap defaultConfig;          // optional per-widget defaults

    // Grid size constraints (replaces WidgetSizeFlags)
    int minCols = 1;
    int minRows = 1;
    int maxCols = 6;
    int maxRows = 4;
    int defaultCols = 1;
    int defaultRows = 1;
};

struct GridPlacement {
    QString instanceId;     // unique per placement
    QString widgetId;       // "org.openauto.clock"
    int col = 0;
    int row = 0;
    int colSpan = 1;
    int rowSpan = 1;
    double opacity = 0.25;  // glass card opacity
    int page = 0;           // which grid page this placement belongs to
    bool visible = true;    // false when clamped out (kept in config)
};

// Legacy types -- used by WidgetPlacementModel and YamlConfig until Plan 02 replaces them.
// Do NOT use in new code. Use GridPlacement and the new grid config API instead.
struct PageDescriptor {
    QString id;
    QString layoutTemplate = QStringLiteral("standard-3pane");
    int order = 0;
    QVariantMap flags;
};

// Legacy pane-based placement -- used by WidgetPlacementModel until Plan 02 replaces it.
// Do NOT use in new code. Use GridPlacement instead.
struct WidgetPlacement {
    QString instanceId;
    QString widgetId;
    QString pageId;
    QString paneId;
    QVariantMap config;

    QString compositeKey() const { return pageId + ":" + paneId; }
};

} // namespace oap
