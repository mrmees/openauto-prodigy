#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVariantMap>
#include <QList>
#include <QString>
#include <QPointer>

namespace oap {

class ActionRegistry;

class OverlayService : public QAbstractListModel {
    Q_OBJECT
public:
    enum class ZBand { Notifications = 1000, User = 2000, SystemModal = 3000, Gesture = 4000 };
    Q_ENUM(ZBand)

    struct OverlayDescriptor {
        QString id;              // "pairing"; reverse-dns for plugin overlays
        QString sourcePluginId;  // "" for system overlays
        QUrl qmlComponent;
        ZBand band = ZBand::User;
        bool visible = false;
        QVariantMap geometry;    // optional {x,y,width,height}; empty = component self-anchors
    };

    enum Roles { OverlayIdRole = Qt::UserRole + 1, QmlComponentRole, ZRole,
                 VisibleRole, GeometryRole };

    explicit OverlayService(ActionRegistry* actions, QObject* parent = nullptr);
    ~OverlayService() override;

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool registerOverlay(const OverlayDescriptor& descriptor);  // false on duplicate id
    void unregisterOverlay(const QString& id);
    Q_INVOKABLE void setVisible(const QString& id, bool visible);
    Q_INVOKABLE void toggle(const QString& id);
    Q_INVOKABLE void move(const QString& id, const QVariantMap& geometry);
    Q_INVOKABLE bool isVisible(const QString& id) const;

signals:
    void overlayVisibilityChanged(const QString& id, bool visible);

private:
    struct Entry {
        OverlayDescriptor desc;
        int z = 0;
    };

    int findOverlay(const QString& id) const;
    void renormalizeBand(ZBand band);
    void unregisterOverlayActions(const QString& id);
    QPointer<ActionRegistry> actions_;
    QList<Entry> overlays_;
};

} // namespace oap
