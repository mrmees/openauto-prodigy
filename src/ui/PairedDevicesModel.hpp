#pragma once

#include <QAbstractListModel>
#include <QList>

namespace oap {

struct PairedDeviceInfo {
    QString address;
    QString name;
    bool connected = false;
};

class PairedDevicesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        AddressRole = Qt::UserRole + 1,
        NameRole,
        ConnectedRole
    };

    explicit PairedDevicesModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDevices(const QList<PairedDeviceInfo>& devices);
    void updateConnectionState(const QString& address, bool connected);

signals:
    void countChanged();

private:
    QList<PairedDeviceInfo> devices_;
};

} // namespace oap
