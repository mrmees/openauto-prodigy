#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QAbstractListModel;

namespace oap {

class IBluetoothService {
public:
    virtual ~IBluetoothService() = default;

    // --- Adapter info ---
    virtual QString adapterAddress() const = 0;
    virtual QString adapterAlias() const = 0;

    // --- Discoverability (always on; informational) ---
    virtual bool isDiscoverable() const = 0;

    // --- Pairable control ---
    virtual bool isPairable() const = 0;
    virtual void setPairable(bool enabled) = 0;

    // --- Pairing dialog state ---
    virtual bool isPairingActive() const = 0;
    virtual QString pairingDeviceName() const = 0;
    virtual QString pairingPasskey() const = 0;
    virtual void confirmPairing() = 0;
    virtual void rejectPairing() = 0;

    // --- Paired devices ---
    virtual QAbstractListModel* pairedDevicesModel() = 0;
    virtual void forgetDevice(const QString& address) = 0;

    // --- Auto-connect ---
    virtual void startAutoConnect() = 0;
    virtual void cancelAutoConnect() = 0;

    // --- Connection state ---
    virtual QString connectedDeviceName() const = 0;
    virtual QString connectedDeviceAddress() const = 0;

    // --- Lifecycle ---
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
};

} // namespace oap
