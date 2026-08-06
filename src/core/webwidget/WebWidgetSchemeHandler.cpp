#include "core/webwidget/WebWidgetSchemeHandler.hpp"

#include <QFile>
#include <QUrl>
#include <QWebEngineUrlRequestJob>

namespace oap {

QWebEngineUrlScheme::Flags webWidgetSchemeFlags()
{
    return QWebEngineUrlScheme::SecureScheme
        | QWebEngineUrlScheme::FetchApiAllowed;
}

WebWidgetSchemeHandler::WebWidgetSchemeHandler(WebWidgetContentResolver* resolver,
                                               QObject* parent)
    : QWebEngineUrlSchemeHandler(parent), resolver_(resolver) {}

void WebWidgetSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job)
{
    const QUrl url = job->requestUrl();
    if (url.host() != QLatin1String("widgets")) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QString path = url.path();                 // "/<id>/<rel...>"
    const int slash = path.indexOf(u'/', 1);
    if (slash < 0) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QString id = path.mid(1, slash - 1);
    const QString rel = path.mid(slash + 1);
    const QString file = resolver_->resolve(id, rel);
    if (file.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    auto* f = new QFile(file, job);                  // job owns the device
    if (!f->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::RequestFailed);
        return;
    }
    job->reply(WebWidgetContentResolver::contentTypeFor(file), f);
}

} // namespace oap
