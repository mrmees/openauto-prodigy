#include "ui/AudioDeviceModel.hpp"

namespace oap {

AudioDeviceModel::AudioDeviceModel(DeviceType type, PipeWireDeviceRegistry* registry,
                                   QObject* parent)
    : QAbstractListModel(parent)
    , type_(type)
    , registry_(registry)
{
    connect(registry_, &PipeWireDeviceRegistry::deviceAdded,
            this, &AudioDeviceModel::onDeviceAdded);
    connect(registry_, &PipeWireDeviceRegistry::deviceRemoved,
            this, &AudioDeviceModel::onDeviceRemoved);

    rebuild();
}

int AudioDeviceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return devices_.size();
}

QVariant AudioDeviceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= devices_.size())
        return {};

    const auto& dev = devices_.at(index.row());
    switch (role) {
    case NodeNameRole:
        return dev.nodeName;
    case DescriptionRole:
    case Qt::DisplayRole:
        return dev.description;
    default:
        return {};
    }
}

QHash<int, QByteArray> AudioDeviceModel::roleNames() const
{
    return {
        { NodeNameRole, "nodeName" },
        { DescriptionRole, "description" },
    };
}

int AudioDeviceModel::indexOfDevice(const QString& nodeName) const
{
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_.at(i).nodeName == nodeName)
            return i;
    }
    return -1;
}

void AudioDeviceModel::onDeviceAdded(const oap::AudioDeviceInfo& /*info*/)
{
    beginResetModel();
    rebuild();
    endResetModel();
    emit countChanged();
}

void AudioDeviceModel::onDeviceRemoved(uint32_t /*registryId*/)
{
    beginResetModel();
    rebuild();
    endResetModel();
    emit countChanged();
}

void AudioDeviceModel::rebuild()
{
    devices_.clear();

    // Get real devices from registry
    const auto devs = (type_ == Output) ? registry_->outputDevices()
                                        : registry_->inputDevices();

    // Synthetic "auto" entry at index 0, showing which device PipeWire will use
    AudioDeviceInfo autoEntry;
    autoEntry.registryId = 0;
    autoEntry.nodeName = "auto";
    autoEntry.description = devs.isEmpty()
        ? QStringLiteral("Default (no devices)")
        : QStringLiteral("Default (%1)").arg(devs.first().description);
    autoEntry.mediaClass = (type_ == Output) ? "Audio/Sink" : "Audio/Source";
    devices_.append(autoEntry);

    devices_.append(devs);
}

} // namespace oap
