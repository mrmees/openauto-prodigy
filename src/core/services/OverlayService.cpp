#include "OverlayService.hpp"
#include "ActionRegistry.hpp"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcOverlay, "oap.overlay")

namespace oap {

OverlayService::OverlayService(ActionRegistry* actions, QObject* parent)
    : QAbstractListModel(parent), actions_(actions)
{
}

int OverlayService::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : overlays_.size();
}

QVariant OverlayService::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= overlays_.size()) return {};
    const auto& e = overlays_[index.row()];
    switch (role) {
    case OverlayIdRole: return e.desc.id;
    case QmlComponentRole: return e.desc.qmlComponent;
    case ZRole: return e.z;
    case VisibleRole: return e.desc.visible;
    case GeometryRole: return e.desc.geometry;
    }
    return {};
}

QHash<int, QByteArray> OverlayService::roleNames() const
{
    return { {OverlayIdRole, "overlayId"}, {QmlComponentRole, "qmlComponent"},
             {ZRole, "z"}, {VisibleRole, "overlayVisible"}, {GeometryRole, "geometry"} };
}

bool OverlayService::registerOverlay(const OverlayDescriptor& descriptor)
{
    if (descriptor.id.isEmpty() || findOverlay(descriptor.id) >= 0) {
        qCWarning(lcOverlay) << "registerOverlay rejected:" << descriptor.id;
        return false;
    }
    Entry e;
    e.desc = descriptor;
    // Fixed band base + intra-band registration order (design §4.1)
    int intra = 0;
    for (const auto& other : overlays_)
        if (other.desc.band == descriptor.band) ++intra;
    e.z = static_cast<int>(descriptor.band) + intra;

    beginInsertRows({}, overlays_.size(), overlays_.size());
    overlays_.append(e);
    endInsertRows();

    if (actions_) {
        const QString base = QStringLiteral("overlay.%1.").arg(descriptor.id);
        const QString id = descriptor.id;
        actions_->registerAction(base + "show",   [this, id](const QVariant&) { setVisible(id, true); });
        actions_->registerAction(base + "hide",   [this, id](const QVariant&) { setVisible(id, false); });
        actions_->registerAction(base + "toggle", [this, id](const QVariant&) { toggle(id); });
        actions_->registerAction(base + "move",   [this, id](const QVariant& v) { move(id, v.toMap()); });
    }
    qCInfo(lcOverlay) << "Overlay registered:" << descriptor.id
                      << "band" << static_cast<int>(descriptor.band) << "z" << e.z;
    return true;
}

void OverlayService::unregisterOverlay(const QString& id)
{
    const int i = findOverlay(id);
    if (i < 0) return;
    if (actions_) {
        const QString base = QStringLiteral("overlay.%1.").arg(id);
        for (const char* suffix : {"show", "hide", "toggle", "move"})
            actions_->unregisterAction(base + suffix);
    }
    beginRemoveRows({}, i, i);
    overlays_.removeAt(i);
    endRemoveRows();
}

void OverlayService::setVisible(const QString& id, bool visible)
{
    const int i = findOverlay(id);
    if (i < 0) { qCWarning(lcOverlay) << "setVisible: unknown overlay" << id; return; }
    if (overlays_[i].desc.visible == visible) return;
    overlays_[i].desc.visible = visible;
    const auto idx = index(i, 0);
    emit dataChanged(idx, idx, {VisibleRole});
    emit overlayVisibilityChanged(id, visible);
}

void OverlayService::toggle(const QString& id)
{
    const int i = findOverlay(id);
    if (i >= 0) setVisible(id, !overlays_[i].desc.visible);
}

void OverlayService::move(const QString& id, const QVariantMap& geometry)
{
    const int i = findOverlay(id);
    if (i < 0) return;
    overlays_[i].desc.geometry = geometry;
    const auto idx = index(i, 0);
    emit dataChanged(idx, idx, {GeometryRole});
}

bool OverlayService::isVisible(const QString& id) const
{
    const int i = findOverlay(id);
    return i >= 0 && overlays_[i].desc.visible;
}

int OverlayService::findOverlay(const QString& id) const
{
    for (int i = 0; i < overlays_.size(); ++i)
        if (overlays_[i].desc.id == id) return i;
    return -1;
}

} // namespace oap
