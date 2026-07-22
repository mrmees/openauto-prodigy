#pragma once

#include <QObject>
#include <atomic>
#include <cstdint>
#include <map>
#include <pipewire/pipewire.h>

namespace oap {

/// Watches the PipeWire registry for HFP SCO nodes
/// (api.bluez5.profile == "headset-audio-gateway") and reports whether any
/// is in the RUNNING state. Node state RUNNING is the reliable "call audio
/// is flowing" signal (HFP decision record §6.3); the D-Bus transport State
/// is advisory only.
///
/// All PipeWire callbacks run on the PW thread loop; the signal is
/// marshaled to the Qt main thread. If PipeWire is unavailable, the monitor
/// stays inert and scoRunning() is permanently false.
class ScoNodeMonitor : public QObject {
    Q_OBJECT
public:
    explicit ScoNodeMonitor(QObject* parent = nullptr);
    ~ScoNodeMonitor() override;

    /// Caller must NOT hold the thread-loop lock (start takes it itself).
    void start(struct pw_thread_loop* loop, struct pw_core* core);
    void stop();

    bool scoRunning() const { return anyRunning_.load(); }

signals:
    void scoRunningChanged(bool running);

private:
    struct Tracked {
        ScoNodeMonitor* owner = nullptr;
        uint32_t id = 0;
        struct pw_node* node = nullptr;
        struct spa_hook listener{};
        bool running = false;
    };

    static void onGlobal(void* data, uint32_t id, uint32_t permissions,
                         const char* type, uint32_t version,
                         const struct spa_dict* props);
    static void onGlobalRemove(void* data, uint32_t id);
    static void onNodeInfo(void* data, const struct pw_node_info* info);
    void recomputeRunning();   // PW thread only
    void destroyTracked(Tracked* t);  // PW thread or under loop lock

    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_registry* registry_ = nullptr;
    struct spa_hook registryListener_{};
    std::map<uint32_t, Tracked*> tracked_;   // PW thread only (+ stop() under lock)
    std::atomic<bool> anyRunning_{false};
    std::atomic<bool> active_{false};
    std::atomic<uint64_t> epoch_{0};
};

} // namespace oap
