#include "core/webwidget/WebWidgetContentResolver.hpp"

#include <QFileInfo>

namespace oap {

void WebWidgetContentResolver::registerPackage(const QString& id, const QString& dirPath)
{
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    if (!canonical.isEmpty())
        packages_.insert(id, canonical);
}

QString WebWidgetContentResolver::resolve(const QString& id,
                                          const QString& relativePath) const
{
    const QString base = packages_.value(id);
    if (base.isEmpty())
        return {};
    // canonicalFilePath() resolves ".." and symlinks; empty if missing.
    const QString file = QFileInfo(base + u'/' + relativePath).canonicalFilePath();
    if (file.isEmpty() || !file.startsWith(base + u'/'))
        return {};
    return file;
}

QByteArray WebWidgetContentResolver::contentTypeFor(const QString& filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == u"html" || ext == u"htm") return QByteArrayLiteral("text/html");
    if (ext == u"js")    return QByteArrayLiteral("application/javascript");
    if (ext == u"css")   return QByteArrayLiteral("text/css");
    if (ext == u"svg")   return QByteArrayLiteral("image/svg+xml");
    if (ext == u"png")   return QByteArrayLiteral("image/png");
    if (ext == u"jpg" || ext == u"jpeg") return QByteArrayLiteral("image/jpeg");
    if (ext == u"woff2") return QByteArrayLiteral("font/woff2");
    if (ext == u"json")  return QByteArrayLiteral("application/json");
    return QByteArrayLiteral("application/octet-stream");
}

} // namespace oap
