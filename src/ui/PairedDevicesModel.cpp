#include "ui/PairedDevicesModel.hpp"

namespace oap {

PairedDevicesModel::PairedDevicesModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PairedDevicesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return devices_.size();
}

QVariant PairedDevicesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= devices_.size())
        return {};

    const auto& dev = devices_[index.row()];
    switch (role) {
    case AddressRole:
        return dev.address;
    case NameRole:
        return dev.name;
    case ConnectedRole:
        return dev.connected;
    default:
        return {};
    }
}

QHash<int, QByteArray> PairedDevicesModel::roleNames() const
{
    return {
        {AddressRole, "address"},
        {NameRole, "name"},
        {ConnectedRole, "connected"},
    };
}

void PairedDevicesModel::setDevices(const QList<PairedDeviceInfo>& devices)
{
    beginResetModel();
    devices_ = devices;
    endResetModel();
    emit countChanged();
}

void PairedDevicesModel::updateConnectionState(const QString& address, bool connected)
{
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_[i].address == address) {
            devices_[i].connected = connected;
            QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx, {ConnectedRole});
            return;
        }
    }
}

} // namespace oap
