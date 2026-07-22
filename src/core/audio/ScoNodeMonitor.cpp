#include "ScoNodeMonitor.hpp"
#include <spa/utils/dict.h>
#include <pipewire/keys.h>
#include <QLoggingCategory>
#include <QThread>
#include <cstring>

Q_LOGGING_CATEGORY(lcSco, "oap.sco")

namespace oap {

ScoNodeMonitor::ScoNodeMonitor(QObject* parent) : QObject(parent) {}

ScoNodeMonitor::~ScoNodeMonitor() { stop(); }

void ScoNodeMonitor::start(struct pw_thread_loop* loop, struct pw_core* core)
{
    if (!loop || !core) {
        qCInfo(lcSco) << "PipeWire unavailable — SCO monitor inert";
        return;
    }
    if (active_.load(std::memory_order_acquire)) return;  // already started
    if (threadLoop_ || registry_)
        stop();

    threadLoop_ = loop;

    pw_thread_loop_lock(threadLoop_);
    registry_ = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (registry_) {
        static const struct pw_registry_events registryEvents = {
            .version = PW_VERSION_REGISTRY_EVENTS,
            .global = onGlobal,
            .global_remove = onGlobalRemove,
        };
        spa_zero(registryListener_);
        const int listenerResult = pw_registry_add_listener(
            registry_, &registryListener_, &registryEvents, this);
        if (listenerResult < 0) {
            qCWarning(lcSco) << "Failed to add PipeWire registry listener:"
                             << listenerResult;
            pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(registry_));
            registry_ = nullptr;
        } else {
            epoch_.fetch_add(1, std::memory_order_acq_rel);
            active_.store(true, std::memory_order_release);
        }
    }
    pw_thread_loop_unlock(threadLoop_);

    if (!registry_)
        threadLoop_ = nullptr;
}

void ScoNodeMonitor::stop()
{
    Q_ASSERT_X(QThread::currentThread() == thread(), "ScoNodeMonitor::stop",
               "stop must run on the monitor's Qt thread");

    active_.store(false, std::memory_order_release);
    epoch_.fetch_add(1, std::memory_order_acq_rel);

    auto* loop = threadLoop_;
    if (loop) {
        pw_thread_loop_lock(loop);
        for (auto& [id, t] : tracked_)
            destroyTracked(t);
        tracked_.clear();
        if (registry_) {
            spa_hook_remove(&registryListener_);
            pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(registry_));
        }
        registry_ = nullptr;
        pw_thread_loop_unlock(loop);
    }

    registry_ = nullptr;
    threadLoop_ = nullptr;

    // A true state is consumer-visible either through a delivered signal or a
    // connect-then-snapshot read of scoRunning(). Clear both sources before
    // emitting so recursive and repeated stop calls remain idempotent.
    const bool observedRunning =
        anyRunning_.exchange(false, std::memory_order_acq_rel);
    const bool emitFallingEdge = observedRunning || lastDeliveredRunning_;
    lastDeliveredRunning_ = false;
    if (emitFallingEdge)
        emit scoRunningChanged(false);
}

void ScoNodeMonitor::destroyTracked(Tracked* t)
{
    spa_hook_remove(&t->listener);
    if (t->node)
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(t->node));
    delete t;
}

void ScoNodeMonitor::onGlobal(void* data, uint32_t id, uint32_t /*permissions*/,
                              const char* type, uint32_t /*version*/,
                              const struct spa_dict* props)
{
    auto* self = static_cast<ScoNodeMonitor*>(data);
    if (!self->active_.load(std::memory_order_acquire))
        return;
    if (!type || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0 || !props)
        return;

    // Match on the bluez profile property alone — do NOT filter on
    // media.class (SCO node classes vary; the profile string is the
    // discriminator, per the live inspection).
    const char* profile = spa_dict_lookup(props, "api.bluez5.profile");
    if (!profile || std::strcmp(profile, "headset-audio-gateway") != 0)
        return;

    auto* t = new Tracked{};
    t->owner = self;
    t->id = id;
    t->node = static_cast<struct pw_node*>(
        pw_registry_bind(self->registry_, id, type, PW_VERSION_NODE, 0));
    if (!t->node) { delete t; return; }

    static const struct pw_node_events nodeEvents = {
        .version = PW_VERSION_NODE_EVENTS,
        .info = onNodeInfo,
    };
    pw_node_add_listener(t->node, &t->listener, &nodeEvents, t);
    self->tracked_[id] = t;
    qCInfo(lcSco) << "Tracking SCO node" << id;
}

void ScoNodeMonitor::onGlobalRemove(void* data, uint32_t id)
{
    auto* self = static_cast<ScoNodeMonitor*>(data);
    auto it = self->tracked_.find(id);
    if (it == self->tracked_.end()) return;
    self->destroyTracked(it->second);
    self->tracked_.erase(it);
    qCInfo(lcSco) << "SCO node removed" << id;
    self->recomputeRunning();
}

void ScoNodeMonitor::onNodeInfo(void* data, const struct pw_node_info* info)
{
    auto* t = static_cast<Tracked*>(data);
    if (!(info->change_mask & PW_NODE_CHANGE_MASK_STATE)) return;
    const bool running = (info->state == PW_NODE_STATE_RUNNING);
    if (running == t->running) return;
    t->running = running;
    t->owner->recomputeRunning();
}

void ScoNodeMonitor::recomputeRunning()
{
    if (!active_.load(std::memory_order_acquire))
        return;

    bool any = false;
    for (const auto& [id, t] : tracked_)
        if (t->running) { any = true; break; }

    updateRunning(any);
}

void ScoNodeMonitor::updateRunning(bool running)
{
    const bool prev = anyRunning_.exchange(running, std::memory_order_acq_rel);
    if (prev == running) return;

    const uint64_t deliveryEpoch = epoch_.load(std::memory_order_acquire);
    QMetaObject::invokeMethod(this, [this, running, deliveryEpoch]() {
        if (!active_.load(std::memory_order_acquire)
            || epoch_.load(std::memory_order_acquire) != deliveryEpoch)
            return;
        if (lastDeliveredRunning_ == running)
            return;
        lastDeliveredRunning_ = running;
        qCInfo(lcSco) << "SCO running:" << running;
        emit scoRunningChanged(running);
    }, Qt::QueuedConnection);
}

} // namespace oap
