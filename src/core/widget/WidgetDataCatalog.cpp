#include "core/widget/WidgetDataCatalog.hpp"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <yaml-cpp/yaml.h>

namespace oap {

namespace {

bool isDescendant(const QString& path, const QString& base)
{
    return path.startsWith(base + QDir::separator());
}

void warnSkipped(const QString& itemPath, const char* reason)
{
    qWarning() << "WidgetDataCatalog: skipping" << itemPath << "—" << reason;
}

} // namespace

WidgetDataCatalog::WidgetDataCatalog(QString rootPath)
{
    setRootPath(rootPath);
}

void WidgetDataCatalog::setRootPath(const QString& rootPath)
{
    rootPath_ = rootPath.isEmpty()
        ? QString()
        : QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
}

bool WidgetDataCatalog::isSafeId(const QString& id)
{
    static const QRegularExpression safeId(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*\\z"));
    return safeId.match(id).hasMatch();
}

QList<WidgetDataItem> WidgetDataCatalog::items(
    const QString& widgetId, const QString& collectionId) const
{
    QList<WidgetDataItem> result;
    if (rootPath_.isEmpty() || !isSafeId(widgetId) || !isSafeId(collectionId))
        return result;

    const QFileInfo rootInfo(rootPath_);
    const QString root = rootInfo.canonicalFilePath();
    if (root.isEmpty() || !rootInfo.isDir())
        return result;

    const QFileInfo widgetInfo(rootPath_ + u'/' + widgetId);
    if (widgetInfo.isSymLink())
        return result;
    const QString widgetRoot = widgetInfo.canonicalFilePath();
    if (widgetRoot.isEmpty() || !widgetInfo.isDir() || !isDescendant(widgetRoot, root))
        return result;

    const QFileInfo collectionInfo(widgetRoot + u'/' + collectionId);
    if (collectionInfo.isSymLink())
        return result;
    const QString collectionRoot = collectionInfo.canonicalFilePath();
    if (collectionRoot.isEmpty() || !collectionInfo.isDir()
        || !isDescendant(collectionRoot, widgetRoot)) {
        return result;
    }

    QDir collectionDir(collectionRoot);
    const auto entries = collectionDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& entry : entries) {
        const QString itemId = entry.fileName();
        if (!isSafeId(itemId) || entry.isSymLink()) {
            warnSkipped(entry.absoluteFilePath(), "unsafe or symlinked item directory");
            continue;
        }

        const QString itemRoot = entry.canonicalFilePath();
        if (itemRoot.isEmpty() || !isDescendant(itemRoot, collectionRoot)) {
            warnSkipped(entry.absoluteFilePath(), "item escapes collection root");
            continue;
        }

        const QFileInfo metadataInfo(itemRoot + QStringLiteral("/item.yaml"));
        if (!metadataInfo.exists() || !metadataInfo.isFile() || metadataInfo.isSymLink()) {
            warnSkipped(itemRoot, "missing or symlinked item.yaml");
            continue;
        }
        const QString metadataPath = metadataInfo.canonicalFilePath();
        if (metadataPath.isEmpty() || !isDescendant(metadataPath, itemRoot)) {
            warnSkipped(itemRoot, "item.yaml escapes item root");
            continue;
        }

        try {
            const YAML::Node rootNode = YAML::LoadFile(metadataPath.toStdString());
            const QString declaredId = QString::fromStdString(
                rootNode["id"].as<std::string>("")).trimmed();
            const QString name = QString::fromStdString(
                rootNode["name"].as<std::string>("")).trimmed();
            if (declaredId != itemId || !isSafeId(declaredId) || name.isEmpty()) {
                warnSkipped(itemRoot, "invalid id or name");
                continue;
            }

            WidgetDataItem item;
            item.id = declaredId;
            item.name = name;
            item.description = QString::fromStdString(
                rootNode["description"].as<std::string>("")).trimmed();
            result.append(std::move(item));
        } catch (const YAML::Exception&) {
            warnSkipped(itemRoot, "malformed item.yaml");
        }
    }

    std::sort(result.begin(), result.end(),
              [](const WidgetDataItem& left, const WidgetDataItem& right) {
        const int nameOrder = QString::compare(
            left.name.toCaseFolded(), right.name.toCaseFolded(), Qt::CaseSensitive);
        if (nameOrder != 0)
            return nameOrder < 0;
        return QString::compare(left.id, right.id, Qt::CaseSensitive) < 0;
    });
    return result;
}

} // namespace oap
