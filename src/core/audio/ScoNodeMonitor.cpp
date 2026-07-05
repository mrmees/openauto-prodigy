#include "ScoNodeMonitor.hpp"
#include <spa/utils/dict.h>
#include <pipewire/keys.h>
#include <QLoggingCategory>
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
    if (registry_) return;  // already started
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
        pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
    }
    pw_thread_loop_unlock(threadLoop_);
}

void ScoNodeMonitor::stop()
{
    if (!registry_ || !threadLoop_) return;
    pw_thread_loop_lock(threadLoop_);
    for (auto& [id, t] : tracked_)
        destroyTracked(t);
    tracked_.clear();
    spa_hook_remove(&registryListener_);
    pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(registry_));
    registry_ = nullptr;
    pw_thread_loop_unlock(threadLoop_);
    threadLoop_ = nullptr;
    anyRunning_.store(false);
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
    bool any = false;
    for (const auto& [id, t] : tracked_)
        if (t->running) { any = true; break; }

    const bool prev = anyRunning_.exchange(any);
    if (prev == any) return;

    QMetaObject::invokeMethod(this, [this, any]() {
        qCInfo(lcSco) << "SCO running:" << any;
        emit scoRunningChanged(any);
    }, Qt::QueuedConnection);
}

} // namespace oap
