#include "BluetoothManager.hpp"
#include "IConfigService.hpp"
#include <QDebug>

namespace oap {

BluetoothManager::BluetoothManager(IConfigService* configService, QObject* parent)
    : QObject(parent)
    , configService_(configService)
{
}

BluetoothManager::~BluetoothManager()
{
    shutdown();
}

QString BluetoothManager::adapterAddress() const { return adapterAddress_; }
QString BluetoothManager::adapterAlias() const { return adapterAlias_; }
bool BluetoothManager::isDiscoverable() const { return discoverable_; }
bool BluetoothManager::isPairable() const { return pairable_; }

void BluetoothManager::setPairable(bool enabled)
{
    if (pairable_ == enabled) return;
    pairable_ = enabled;
    emit pairableChanged();
    qInfo() << "[BtManager] Pairable:" << enabled;
    // TODO: set Adapter1.Pairable via D-Bus
}

bool BluetoothManager::isPairingActive() const { return pairingActive_; }
QString BluetoothManager::pairingDeviceName() const { return pairingDeviceName_; }
QString BluetoothManager::pairingPasskey() const { return pairingPasskey_; }

void BluetoothManager::confirmPairing()
{
    qInfo() << "[BtManager] Pairing confirmed by user";
    // TODO: reply to pending Agent1 D-Bus method call
}

void BluetoothManager::rejectPairing()
{
    qInfo() << "[BtManager] Pairing rejected by user";
    // TODO: reply with rejection to pending Agent1 D-Bus method call
}

QAbstractListModel* BluetoothManager::pairedDevicesModel()
{
    return nullptr;  // TODO: return PairedDevicesModel
}

void BluetoothManager::forgetDevice(const QString& address)
{
    qInfo() << "[BtManager] Forget device:" << address;
    // TODO: call Adapter1.RemoveDevice via D-Bus
}

void BluetoothManager::startAutoConnect()
{
    qInfo() << "[BtManager] Starting auto-connect";
    // TODO: enumerate paired devices, start retry loop
}

void BluetoothManager::cancelAutoConnect()
{
    qInfo() << "[BtManager] Cancelling auto-connect";
    // TODO: stop retry timer
}

QString BluetoothManager::connectedDeviceName() const { return connectedDeviceName_; }
QString BluetoothManager::connectedDeviceAddress() const { return connectedDeviceAddress_; }

void BluetoothManager::initialize()
{
    qInfo() << "[BtManager] Initializing...";
    // TODO: adapter setup, agent registration, profile registration, auto-connect
}

void BluetoothManager::shutdown()
{
    qInfo() << "[BtManager] Shutting down";
    cancelAutoConnect();
    // TODO: unregister agent, profiles
}

} // namespace oap
