#pragma once

#include <QObject>
#include <QVariant>
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
    explicit TouchHandler(QObject* parent = nullptr)
        : QObject(parent) {}

    void setChannel(std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel,
                    boost::asio::io_service::strand* strand) {
        channel_ = std::move(channel);
        strand_ = strand;
    }

    // Single touch: action 0=PRESS, 1=RELEASE, 2=DRAG
    Q_INVOKABLE void sendTouchEvent(int x, int y, int action) {
        sendMultiTouchEvent(x, y, 0, action);
    }

    // Multi-touch: action 0=PRESS, 1=RELEASE, 2=DRAG, 5=POINTER_DOWN, 6=POINTER_UP
    Q_INVOKABLE void sendMultiTouchEvent(int x, int y, int pointerId, int action) {
        if (!channel_ || !strand_) return;

        auto t_qml = PerfStats::Clock::now().time_since_epoch().count();

        strand_->dispatch([this, x, y, pointerId, action, t_qml]() {
            auto t_dispatch = PerfStats::Clock::now();

            aasdk::proto::messages::InputEventIndication indication;
            indication.set_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            indication.set_disp_channel(0);

            auto* touchEvent = indication.mutable_touch_event();
            touchEvent->set_touch_action(
                static_cast<aasdk::proto::enums::TouchAction::Enum>(action));
            touchEvent->set_action_index(pointerId);

            auto* location = touchEvent->add_touch_location();
            location->set_x(x);
            location->set_y(y);
            location->set_pointer_id(pointerId);

            auto promise = aasdk::channel::SendPromise::defer(*strand_);
            promise->then([]() {}, [](const aasdk::error::Error&) {});
            channel_->sendInputEventIndication(indication, std::move(promise));

            auto t_send = PerfStats::Clock::now();
            auto t_qmlPoint = PerfStats::TimePoint(std::chrono::nanoseconds(t_qml));

            metricDispatch_.record(PerfStats::msElapsed(t_qmlPoint, t_dispatch));
            metricSend_.record(PerfStats::msElapsed(t_dispatch, t_send));
            metricTotal_.record(PerfStats::msElapsed(t_qmlPoint, t_send));
            ++eventsSinceLog_;

            double secSinceLog = PerfStats::msElapsed(lastLogTime_, t_send) / 1000.0;
            if (secSinceLog >= 5.0) {
                double eventsPerSec = eventsSinceLog_ / secSinceLog;
                BOOST_LOG_TRIVIAL(info)
                    << "[Perf] Touch: qml->dispatch=" << std::fixed << std::setprecision(1)
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

    Q_INVOKABLE void sendBatchTouchEvent(QVariantList points, int action) {
        if (!channel_ || !strand_ || points.isEmpty()) return;

        auto t_qml = PerfStats::Clock::now().time_since_epoch().count();

        strand_->dispatch([this, points = std::move(points), action, t_qml]() {
            auto t_dispatch = PerfStats::Clock::now();

            aasdk::proto::messages::InputEventIndication indication;
            indication.set_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            indication.set_disp_channel(0);

            auto* touchEvent = indication.mutable_touch_event();
            touchEvent->set_touch_action(
                static_cast<aasdk::proto::enums::TouchAction::Enum>(action));

            if (!points.isEmpty()) {
                auto firstPt = points[0].toMap();
                touchEvent->set_action_index(firstPt["pointerId"].toInt());
            }

            for (const auto& pt : points) {
                auto map = pt.toMap();
                auto* location = touchEvent->add_touch_location();
                location->set_x(map["x"].toInt());
                location->set_y(map["y"].toInt());
                location->set_pointer_id(map["pointerId"].toInt());
            }

            auto promise = aasdk::channel::SendPromise::defer(*strand_);
            promise->then([]() {}, [](const aasdk::error::Error&) {});
            channel_->sendInputEventIndication(indication, std::move(promise));

            auto t_send = PerfStats::Clock::now();
            auto t_qmlPoint = PerfStats::TimePoint(std::chrono::nanoseconds(t_qml));

            metricDispatch_.record(PerfStats::msElapsed(t_qmlPoint, t_dispatch));
            metricSend_.record(PerfStats::msElapsed(t_dispatch, t_send));
            metricTotal_.record(PerfStats::msElapsed(t_qmlPoint, t_send));
            ++eventsSinceLog_;

            double secSinceLog = PerfStats::msElapsed(lastLogTime_, t_send) / 1000.0;
            if (secSinceLog >= 5.0) {
                double eventsPerSec = eventsSinceLog_ / secSinceLog;
                BOOST_LOG_TRIVIAL(info)
                    << "[Perf] Touch: qml->dispatch=" << std::fixed << std::setprecision(1)
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

private:
    std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_;
    boost::asio::io_service::strand* strand_ = nullptr;

    // Performance instrumentation
    PerfStats::Metric metricDispatch_;
    PerfStats::Metric metricSend_;
    PerfStats::Metric metricTotal_;
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t eventsSinceLog_ = 0;
};

} // namespace aa
} // namespace oap
