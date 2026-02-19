#pragma once

#include <QString>
#include <QStringList>

namespace oap {

class IBluetoothService {
public:
    virtual ~IBluetoothService() = default;

    /// Get the local adapter's Bluetooth MAC address.
    /// Returns empty string if no adapter is available.
    /// Thread-safe.
    virtual QString adapterAddress() const = 0;

    /// Whether the adapter is currently discoverable by other devices.
    /// Thread-safe.
    virtual bool isDiscoverable() const = 0;

    /// Enable or disable discoverability.
    /// Must be called from the main thread.
    virtual void setDiscoverable(bool enabled) = 0;

    /// Start BLE advertising for wireless AA discovery.
    /// Must be called from the main thread. Returns true on success.
    virtual bool startAdvertising() = 0;

    /// Stop BLE advertising.
    /// Must be called from the main thread.
    virtual void stopAdvertising() = 0;

    /// List paired device addresses.
    /// Thread-safe.
    virtual QStringList pairedDevices() const = 0;
};

} // namespace oap
