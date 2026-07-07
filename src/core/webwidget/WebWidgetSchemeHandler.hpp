#pragma once

#include <QWebEngineUrlSchemeHandler>

#include "core/webwidget/WebWidgetContentResolver.hpp"

namespace oap {

// Serves prodigy://widgets/<id>/<path> from scanned package directories.
// All decisions live in WebWidgetContentResolver; this class only speaks
// WebEngine (design §3).
class WebWidgetSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT
public:
    explicit WebWidgetSchemeHandler(WebWidgetContentResolver* resolver,
                                    QObject* parent = nullptr);
    void requestStarted(QWebEngineUrlRequestJob* job) override;

private:
    WebWidgetContentResolver* resolver_;
};

} // namespace oap
