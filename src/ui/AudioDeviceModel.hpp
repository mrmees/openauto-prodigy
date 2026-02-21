#pragma once

#include "core/audio/PipeWireDeviceRegistry.hpp"
#include <QAbstractListModel>

namespace oap {

class AudioDeviceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { NodeNameRole = Qt::UserRole + 1, DescriptionRole };

    enum DeviceType { Output, Input };

    AudioDeviceModel(DeviceType type, PipeWireDeviceRegistry* registry,
                     QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOfDevice(const QString& nodeName) const;

signals:
    void countChanged();

private slots:
    void onDeviceAdded(const oap::AudioDeviceInfo& info);
    void onDeviceRemoved(uint32_t registryId);

private:
    void rebuild();
    DeviceType type_;
    PipeWireDeviceRegistry* registry_;
    QList<AudioDeviceInfo> devices_; // index 0 = synthetic "auto" entry
};

} // namespace oap
