#pragma once

#include <QList>
#include <QString>

namespace oap {

struct WidgetDataItem {
    QString id;
    QString name;
    QString description;
};

// Discovers one directory level of generic widget-owned data. The catalog
// reads only item.yaml picker metadata; all other files remain opaque to the
// host and are interpreted by the owning widget.
class WidgetDataCatalog {
public:
    explicit WidgetDataCatalog(QString rootPath = {});

    void setRootPath(const QString& rootPath);
    QString rootPath() const { return rootPath_; }

    QList<WidgetDataItem> items(const QString& widgetId,
                                const QString& collectionId) const;

    static bool isSafeId(const QString& id);

private:
    QString rootPath_;
};

} // namespace oap
