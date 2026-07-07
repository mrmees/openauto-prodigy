#pragma once

#include <QString>

namespace oap {

class WidgetRegistry;
class WebWidgetContentResolver;

// Startup scan of ~/.openauto/webwidgets/ (design §4): each subdirectory
// with a valid widget.yaml becomes a WebWidget WidgetDescriptor hosted by
// WebWidgetHost.qml. Bad packages are logged and skipped — a broken widget
// must never take the launcher down. Scan-once; no hot reload in v1.
class WebWidgetScanner {
public:
    // Returns the number of widgets registered. resolver may be null
    // (tests, non-WebEngine builds) — package dirs are then not served.
    static int scan(const QString& rootDir, WidgetRegistry& registry,
                    WebWidgetContentResolver* resolver);
};

} // namespace oap
