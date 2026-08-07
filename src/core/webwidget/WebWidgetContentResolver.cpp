#include "core/webwidget/WebWidgetContentResolver.hpp"

#include <QDir>
#include <QFileInfo>

namespace oap {

void WebWidgetContentResolver::registerPackage(const QString& id, const QString& dirPath)
{
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    if (!canonical.isEmpty())
        packages_.insert(id, canonical);
}

void WebWidgetContentResolver::setDataRoot(const QString& dirPath)
{
    dataRoot_ = dirPath.isEmpty()
        ? QString()
        : QDir::cleanPath(QFileInfo(dirPath).absoluteFilePath());
}

QString WebWidgetContentResolver::resolve(const QString& id,
                                          const QString& relativePath) const
{
    const QString base = packages_.value(id);
    if (base.isEmpty() || relativePath.isEmpty() || relativePath.startsWith(u'/'))
        return {};

    if (relativePath == QStringLiteral("__data__"))
        return {};
    if (relativePath.startsWith(QStringLiteral("__data__/")))
        return resolveWidgetData(id, relativePath.mid(9));

    // canonicalFilePath() resolves ".." and symlinks; empty if missing.
    const QString file = QFileInfo(base + u'/' + relativePath).canonicalFilePath();
    if (file.isEmpty() || !file.startsWith(base + u'/'))
        return {};
    return file;
}

QString WebWidgetContentResolver::resolveWidgetData(
    const QString& id, const QString& relativePath) const
{
    if (dataRoot_.isEmpty() || relativePath.isEmpty()
        || relativePath.startsWith(u'/')) {
        return {};
    }

    const QStringList components = relativePath.split(u'/', Qt::KeepEmptyParts);
    for (const QString& component : components) {
        if (component.isEmpty() || component == QStringLiteral(".")
            || component == QStringLiteral("..")) {
            return {};
        }
    }

    const QFileInfo rootInfo(dataRoot_);
    const QString root = rootInfo.canonicalFilePath();
    if (root.isEmpty() || !rootInfo.isDir())
        return {};

    const QFileInfo widgetInfo(dataRoot_ + u'/' + id);
    if (widgetInfo.isSymLink())
        return {};
    const QString widgetRoot = widgetInfo.canonicalFilePath();
    if (widgetRoot.isEmpty() || !widgetInfo.isDir()
        || !widgetRoot.startsWith(root + QDir::separator())) {
        return {};
    }

    QString candidate = widgetRoot;
    for (const QString& component : components) {
        candidate += u'/' + component;
        if (QFileInfo(candidate).isSymLink())
            return {};
    }

    const QFileInfo fileInfo(candidate);
    const QString file = fileInfo.canonicalFilePath();
    if (file.isEmpty() || !fileInfo.isFile()
        || !file.startsWith(widgetRoot + QDir::separator())) {
        return {};
    }
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
