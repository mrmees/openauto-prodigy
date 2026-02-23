#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>
#include <atomic>
#include <chrono>
#include <iomanip>
#include "PerfStats.hpp"
#include "handlers/InputChannelHandler.hpp"

namespace oap {
namespace aa {

class TouchHandler : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList debugTouches READ debugTouches NOTIFY debugTouchesChanged)
    Q_PROPERTY(bool debugOverlay READ debugOverlay WRITE setDebugOverlay NOTIFY debugOverlayChanged)
    Q_PROPERTY(int contentWidth READ contentWidth NOTIFY contentDimsChanged)
    Q_PROPERTY(int contentHeight READ contentHeight NOTIFY contentDimsChanged)

public:
    using Pointer = InputChannelHandler::Pointer;

    QVariantList debugTouches() const { return debugTouches_; }
    bool debugOverlay() const { return debugOverlay_; }
    void setDebugOverlay(bool v) { if (debugOverlay_ != v) { debugOverlay_ = v; emit debugOverlayChanged(); } }
    int contentWidth() const { return contentWidth_; }
    int contentHeight() const { return contentHeight_; }
    void setContentDims(int w, int h) {
        if (contentWidth_ != w || contentHeight_ != h) {
            contentWidth_ = w; contentHeight_ = h;
            emit contentDimsChanged();
        }
    }

    explicit TouchHandler(QObject* parent = nullptr)
        : QObject(parent) {}

    // Thread safety: handler_ is written once from main thread before
    // EvdevTouchReader starts, then read from the evdev thread.
    void setHandler(InputChannelHandler* handler) {
        handler_.store(handler, std::memory_order_release);
    }

    // Legacy compat — ServiceFactory still calls this. Will be removed in Phase 5.
    template<typename Channel, typename Strand>
    void setChannel(Channel, Strand) {}

    // Send a complete touch event with all active pointers.
    // action: 0=DOWN, 1=UP, 2=MOVE, 5=POINTER_DOWN, 6=POINTER_UP
    // actionIndex: index into pointers[] of the finger that triggered the action
    // Called from EvdevTouchReader thread — updates debug overlay via Qt main thread
    void sendTouchIndication(int count, const Pointer* pointers, int actionIndex, int action) {
        if (count <= 0) return;

        // Update debug overlay (marshal to Qt main thread)
        if (debugOverlay_) {
            QVariantList touches;
            for (int i = 0; i < count; ++i) {
                QVariantMap pt;
                pt["x"] = pointers[i].x;
                pt["y"] = pointers[i].y;
                pt["id"] = pointers[i].id;
                touches.append(pt);
            }
            // action 1=UP: if last finger lifted, clear overlay
            if (action == 1) touches.clear();
            QMetaObject::invokeMethod(this, [this, touches]() {
                debugTouches_ = touches;
                emit debugTouchesChanged();
            }, Qt::QueuedConnection);
        }

        auto* h = handler_.load(std::memory_order_acquire);
        if (!h) return;

        auto t_start = PerfStats::Clock::now();

        // Generate timestamp in microseconds (matches AA protocol expectation)
        uint64_t timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());

        // InputChannelHandler builds the protobuf and emits sendRequested
        h->sendTouchIndication(count, pointers, actionIndex, action, timestamp);

        auto t_send = PerfStats::Clock::now();

        metricTotal_.record(PerfStats::msElapsed(t_start, t_send));
        ++eventsSinceLog_;

        double secSinceLog = PerfStats::msElapsed(lastLogTime_, t_send) / 1000.0;
        if (secSinceLog >= 5.0) {
            double eventsPerSec = eventsSinceLog_ / secSinceLog;
            qDebug() << "[Perf] Touch: total=" << QString::number(metricTotal_.avg(), 'f', 1) << "ms"
                     << "(p99~" << QString::number(metricTotal_.max, 'f', 1) << "ms)"
                     << "|" << QString::number(eventsPerSec, 'f', 1) << "events/sec";
            metricTotal_.reset();
            eventsSinceLog_ = 0;
            lastLogTime_ = t_send;
        }
    }

    // Convenience for QML single-touch (fallback if evdev not available)
    Q_INVOKABLE void sendTouchEvent(int x, int y, int action) {
        Pointer pt{x, y, 0};
        sendTouchIndication(1, &pt, 0, action);
    }

signals:
    void debugTouchesChanged();
    void debugOverlayChanged();
    void contentDimsChanged();

private:
    QVariantList debugTouches_;
    bool debugOverlay_ = false;
    int contentWidth_ = 1280;
    int contentHeight_ = 720;

    std::atomic<InputChannelHandler*> handler_{nullptr};

    PerfStats::Metric metricTotal_;
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t eventsSinceLog_ = 0;
};

} // namespace aa
} // namespace oap
