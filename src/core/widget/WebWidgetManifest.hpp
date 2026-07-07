#pragma once

#include <QString>

namespace oap {

// Parsed ~/.openauto/webwidgets/<dir>/widget.yaml (design §4). Mirrors the
// WidgetDescriptor fields a web package may set; defaults match
// WidgetDescriptor defaults.
struct WebWidgetManifest {
    QString id;           // required; becomes the prodigy:// URL segment
    QString name;         // required; picker display name
    QString entry = QStringLiteral("index.html");
    QString category = QStringLiteral("status");
    QString description;
    QString icon;         // Material codepoint, native-widget convention
    int minCols = 1;
    int minRows = 1;
    int maxCols = 6;
    int maxRows = 4;
    int defaultCols = 1;
    int defaultRows = 1;
    QString dirPath;      // package directory, set by fromFile()

    static WebWidgetManifest fromFile(const QString& filePath);
    bool isValid() const;
};

} // namespace oap
