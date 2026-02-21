#include "PipeWireDeviceRegistry.hpp"
#include <spa/utils/dict.h>
#include <pipewire/keys.h>
#include <cstring>

namespace oap {

PipeWireDeviceRegistry::PipeWireDeviceRegistry(QObject* parent)
    : QObject(parent)
{
}

PipeWireDeviceRegistry::~PipeWireDeviceRegistry()
{
    stop();
}

void PipeWireDeviceRegistry::start(struct pw_thread_loop* threadLoop, struct pw_core* core)
{
    threadLoop_ = threadLoop;

    registry_ = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (!registry_)
        return;

    static const struct pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = onGlobal,
        .global_remove = onGlobalRemove,
    };

    spa_zero(registryListener_);
    pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
}

void PipeWireDeviceRegistry::stop()
{
    if (registry_) {
        spa_hook_remove(&registryListener_);
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(registry_));
        registry_ = nullptr;
    }
    threadLoop_ = nullptr;

    QMutexLocker lock(&mutex_);
    devices_.clear();
}

QList<AudioDeviceInfo> PipeWireDeviceRegistry::outputDevices() const
{
    QMutexLocker lock(&mutex_);
    QList<AudioDeviceInfo> result;
    for (const auto& dev : devices_) {
        if (dev.mediaClass == QStringLiteral("Audio/Sink") ||
            dev.mediaClass == QStringLiteral("Audio/Duplex")) {
            result.append(dev);
        }
    }
    return result;
}

QList<AudioDeviceInfo> PipeWireDeviceRegistry::inputDevices() const
{
    QMutexLocker lock(&mutex_);
    QList<AudioDeviceInfo> result;
    for (const auto& dev : devices_) {
        if (dev.mediaClass == QStringLiteral("Audio/Source") ||
            dev.mediaClass == QStringLiteral("Audio/Duplex")) {
            result.append(dev);
        }
    }
    return result;
}

void PipeWireDeviceRegistry::testAddDevice(const AudioDeviceInfo& info)
{
    {
        QMutexLocker lock(&mutex_);
        devices_.append(info);
    }
    emit deviceAdded(info);
}

void PipeWireDeviceRegistry::testRemoveDevice(uint32_t registryId)
{
    {
        QMutexLocker lock(&mutex_);
        devices_.removeIf([registryId](const AudioDeviceInfo& d) {
            return d.registryId == registryId;
        });
    }
    emit deviceRemoved(registryId);
}

void PipeWireDeviceRegistry::onGlobal(void* data, uint32_t id, uint32_t /*permissions*/,
                                       const char* type, uint32_t /*version*/,
                                       const struct spa_dict* props)
{
    if (!type || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;
    if (!props)
        return;

    const char* mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!mediaClass)
        return;

    // Only care about audio nodes
    if (std::strcmp(mediaClass, "Audio/Sink") != 0 &&
        std::strcmp(mediaClass, "Audio/Source") != 0 &&
        std::strcmp(mediaClass, "Audio/Duplex") != 0) {
        return;
    }

    const char* nodeName = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* nodeDesc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

    AudioDeviceInfo info;
    info.registryId = id;
    info.nodeName = nodeName ? QString::fromUtf8(nodeName) : QString();
    info.description = nodeDesc ? QString::fromUtf8(nodeDesc) : info.nodeName;
    info.mediaClass = QString::fromUtf8(mediaClass);

    auto* self = static_cast<PipeWireDeviceRegistry*>(data);

    {
        QMutexLocker lock(&self->mutex_);
        self->devices_.append(info);
    }

    QMetaObject::invokeMethod(self, [self, info]() {
        emit self->deviceAdded(info);
    }, Qt::QueuedConnection);
}

void PipeWireDeviceRegistry::onGlobalRemove(void* data, uint32_t id)
{
    auto* self = static_cast<PipeWireDeviceRegistry*>(data);

    bool found = false;
    {
        QMutexLocker lock(&self->mutex_);
        auto sizeBefore = self->devices_.size();
        self->devices_.removeIf([id](const AudioDeviceInfo& d) {
            return d.registryId == id;
        });
        found = (self->devices_.size() != sizeBefore);
    }

    if (found) {
        QMetaObject::invokeMethod(self, [self, id]() {
            emit self->deviceRemoved(id);
        }, Qt::QueuedConnection);
    }
}

void PipeWireDeviceRegistry::addDevice(const AudioDeviceInfo& info)
{
    {
        QMutexLocker lock(&mutex_);
        devices_.append(info);
    }
    emit deviceAdded(info);
}

void PipeWireDeviceRegistry::removeDevice(uint32_t registryId)
{
    {
        QMutexLocker lock(&mutex_);
        devices_.removeIf([registryId](const AudioDeviceInfo& d) {
            return d.registryId == registryId;
        });
    }
    emit deviceRemoved(registryId);
}

} // namespace oap
