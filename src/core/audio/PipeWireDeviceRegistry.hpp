#pragma once

#include "AudioDeviceInfo.hpp"
#include <QObject>
#include <QList>
#include <QMutex>
#include <pipewire/pipewire.h>

namespace oap {

class PipeWireDeviceRegistry : public QObject {
    Q_OBJECT

public:
    explicit PipeWireDeviceRegistry(QObject* parent = nullptr);
    ~PipeWireDeviceRegistry() override;

    void start(struct pw_thread_loop* threadLoop, struct pw_core* core);
    void stop();

    QList<AudioDeviceInfo> outputDevices() const;
    QList<AudioDeviceInfo> inputDevices() const;

    // Test helpers
    void testAddDevice(const AudioDeviceInfo& info);
    void testRemoveDevice(uint32_t registryId);

signals:
    void deviceAdded(const oap::AudioDeviceInfo& info);
    void deviceRemoved(uint32_t registryId);

private:
    static void onGlobal(void* data, uint32_t id, uint32_t permissions,
                         const char* type, uint32_t version,
                         const struct spa_dict* props);
    static void onGlobalRemove(void* data, uint32_t id);

    void addDevice(const AudioDeviceInfo& info);
    void removeDevice(uint32_t registryId);

    mutable QMutex mutex_;
    QList<AudioDeviceInfo> devices_;

    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_registry* registry_ = nullptr;
    struct spa_hook registryListener_{};
};

} // namespace oap
