#pragma once

#include <QObject>
#include <chrono>
#include <boost/asio.hpp>
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

        strand_->dispatch([this, x, y, pointerId, action]() {
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
        });
    }

private:
    std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_;
    boost::asio::io_service::strand* strand_ = nullptr;
};

} // namespace aa
} // namespace oap
