#pragma once

#include <QObject>
#include <vector>
#include <chrono>
#include <iomanip>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include "PerfStats.hpp"
#include <aasdk/Channel/Input/InputServiceChannel.hpp>
#include <aasdk/Channel/Promise.hpp>
#include <aasdk_proto/InputEventIndicationMessage.pb.h>
#include <aasdk_proto/TouchEventData.pb.h>
#include <aasdk_proto/TouchLocationData.pb.h>
#include <aasdk_proto/TouchActionEnum.pb.h>

namespace oap {
namespace aa {

class TouchHandler : public QObject {
    Q_OBJECT

public:
    struct Pointer { int x; int y; int id; };

    explicit TouchHandler(QObject* parent = nullptr)
        : QObject(parent) {}

    void setChannel(std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel,
                    boost::asio::io_service::strand* strand) {
        channel_ = std::move(channel);
        strand_ = strand;
    }

    // Send a complete touch event with all active pointers.
    // action: 0=DOWN, 1=UP, 2=MOVE, 5=POINTER_DOWN, 6=POINTER_UP
    // actionIndex: index into pointers[] of the finger that triggered the action
    void sendTouchIndication(int count, const Pointer* pointers, int actionIndex, int action) {
        if (!channel_ || !strand_ || count <= 0) return;

        // Copy pointer data for the lambda capture
        std::vector<Pointer> pts(pointers, pointers + count);
        auto t_start = PerfStats::Clock::now().time_since_epoch().count();

        strand_->dispatch([this, pts = std::move(pts), actionIndex, action, t_start]() {
            auto t_dispatch = PerfStats::Clock::now();

            aasdk::proto::messages::InputEventIndication indication;
            indication.set_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            indication.set_disp_channel(0);

            auto* touchEvent = indication.mutable_touch_event();
            touchEvent->set_touch_action(
                static_cast<aasdk::proto::enums::TouchAction::Enum>(action));
            touchEvent->set_action_index(actionIndex);

            for (const auto& pt : pts) {
                auto* location = touchEvent->add_touch_location();
                location->set_x(pt.x);
                location->set_y(pt.y);
                location->set_pointer_id(pt.id);
            }

            auto promise = aasdk::channel::SendPromise::defer(*strand_);
            promise->then([]() {}, [](const aasdk::error::Error&) {});
            channel_->sendInputEventIndication(indication, std::move(promise));

            auto t_send = PerfStats::Clock::now();
            auto t_startPt = PerfStats::TimePoint(std::chrono::nanoseconds(t_start));

            metricDispatch_.record(PerfStats::msElapsed(t_startPt, t_dispatch));
            metricSend_.record(PerfStats::msElapsed(t_dispatch, t_send));
            metricTotal_.record(PerfStats::msElapsed(t_startPt, t_send));
            ++eventsSinceLog_;

            double secSinceLog = PerfStats::msElapsed(lastLogTime_, t_send) / 1000.0;
            if (secSinceLog >= 5.0) {
                double eventsPerSec = eventsSinceLog_ / secSinceLog;
                BOOST_LOG_TRIVIAL(info)
                    << "[Perf] Touch: dispatch=" << std::fixed << std::setprecision(1)
                    << metricDispatch_.avg() << "ms"
                    << " send=" << metricSend_.avg() << "ms"
                    << " total=" << metricTotal_.avg() << "ms"
                    << " (p99~" << metricTotal_.max << "ms)"
                    << " | " << std::setprecision(1) << eventsPerSec << " events/sec";
                metricDispatch_.reset();
                metricSend_.reset();
                metricTotal_.reset();
                eventsSinceLog_ = 0;
                lastLogTime_ = t_send;
            }
        });
    }

    // Convenience for QML single-touch (fallback if evdev not available)
    Q_INVOKABLE void sendTouchEvent(int x, int y, int action) {
        Pointer pt{x, y, 0};
        sendTouchIndication(1, &pt, 0, action);
    }

private:
    std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_;
    boost::asio::io_service::strand* strand_ = nullptr;

    PerfStats::Metric metricDispatch_;
    PerfStats::Metric metricSend_;
    PerfStats::Metric metricTotal_;
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t eventsSinceLog_ = 0;
};

} // namespace aa
} // namespace oap
