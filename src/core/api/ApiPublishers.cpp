#include "core/api/ApiPublishers.hpp"
#include "core/api/ApiSerializers.hpp"

#include "core/services/IMediaStatusProvider.hpp"
#include "core/services/INavigationProvider.hpp"
#include "core/services/IProjectionStatusProvider.hpp"
#include "core/services/IPhoneStateService.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/BluetoothManager.hpp"
#include "ui/DisplayInfo.hpp"

#include <QDateTime>
#include <string>

namespace oap::api {

namespace {
QByteArray toBytes(const prodigy::api::v1::ApiMessage& m) {
    std::string s;
    m.SerializeToString(&s);
    return QByteArray(s.data(), static_cast<int>(s.size()));
}
} // namespace

// ---- TopicPublisher --------------------------------------------------------

TopicPublisher::TopicPublisher(prodigy::api::v1::Topic topic, QObject* parent)
    : QObject(parent), topic_(topic)
{
    coalesce_.setSingleShot(true);
    connect(&coalesce_, &QTimer::timeout, this, &TopicPublisher::emitNow);
}

QByteArray TopicPublisher::snapshotBytes() {
    return toBytes(buildEnvelope());
}

void TopicPublisher::scheduleEmit() {
    if (!coalesce_.isActive())
        coalesce_.start(0);
}

void TopicPublisher::emitNow() {
    emit statusReady(topic_, toBytes(buildEnvelope()));
}

// ---- MediaPublisher ---------------------------------------------------------

MediaPublisher::MediaPublisher(oap::IMediaStatusProvider* p, QObject* parent)
    : TopicPublisher(prodigy::api::v1::TOPIC_MEDIA, parent), p_(p)
{
    connect(p_, &oap::IMediaStatusProvider::mediaStatusChanged, this, [this] { scheduleEmit(); });
}

prodigy::api::v1::ApiMessage MediaPublisher::buildEnvelope() {
    prodigy::api::v1::ApiMessage m;
    m.set_request_id(0);
    *m.mutable_media_status() = oap::api::serial::buildMediaStatus(*p_);
    return m;
}

// ---- NavigationPublisher -----------------------------------------------------

NavigationPublisher::NavigationPublisher(oap::INavigationProvider* p, QObject* parent)
    : TopicPublisher(prodigy::api::v1::TOPIC_NAVIGATION, parent), p_(p)
{
    connect(p_, &oap::INavigationProvider::navActiveChanged, this, [this] { scheduleEmit(); });
    connect(p_, &oap::INavigationProvider::turnDataChanged, this, [this] { scheduleEmit(); });
    connect(p_, &oap::INavigationProvider::distanceChanged, this, [this] { scheduleEmit(); });
}

prodigy::api::v1::ApiMessage NavigationPublisher::buildEnvelope() {
    prodigy::api::v1::ApiMessage m;
    m.set_request_id(0);
    *m.mutable_navigation_status() = oap::api::serial::buildNavigationStatus(*p_);
    return m;
}

// ---- ProjectionPublisher ------------------------------------------------------

ProjectionPublisher::ProjectionPublisher(oap::IProjectionStatusProvider* p, QObject* parent)
    : TopicPublisher(prodigy::api::v1::TOPIC_PROJECTION, parent), p_(p)
{
    connect(p_, &oap::IProjectionStatusProvider::projectionStateChanged, this, [this] { scheduleEmit(); });
    connect(p_, &oap::IProjectionStatusProvider::statusMessageChanged, this, [this] { scheduleEmit(); });
}

prodigy::api::v1::ApiMessage ProjectionPublisher::buildEnvelope() {
    prodigy::api::v1::ApiMessage m;
    m.set_request_id(0);
    *m.mutable_projection_status() = oap::api::serial::buildProjectionStatus(*p_);
    return m;
}

// ---- SystemPublisher -----------------------------------------------------------

SystemPublisher::SystemPublisher(oap::ThemeService* theme, QString appVersion,
                                  oap::BluetoothManager* bt, oap::DisplayInfo* display,
                                  QObject* parent)
    : TopicPublisher(prodigy::api::v1::TOPIC_SYSTEM, parent),
      theme_(theme), appVersion_(std::move(appVersion)), bt_(bt), display_(display)
{
    connect(theme_, &oap::ThemeService::modeChanged, this, [this] { scheduleEmit(); });
    connect(theme_, &oap::ThemeService::colorsChanged, this, [this] { scheduleEmit(); });
    connect(theme_, &oap::ThemeService::currentThemeIdChanged, this, [this] { scheduleEmit(); });
    if (bt_) {
        connect(bt_, &oap::BluetoothManager::connectedDeviceChanged, this, [this] { scheduleEmit(); });
    }
    if (display_) {
        // Both windowWidth/windowHeight NOTIFY through the same signal --
        // one connection re-publishes on any window resize.
        connect(display_, &oap::DisplayInfo::windowSizeChanged, this, [this] { scheduleEmit(); });
    }
}

prodigy::api::v1::ApiMessage SystemPublisher::buildEnvelope() {
    prodigy::api::v1::ApiMessage m;
    m.set_request_id(0);
    *m.mutable_system_status() = oap::api::serial::buildSystemStatus(*theme_, appVersion_, bt_, display_);
    return m;
}

// ---- PhonePublisher --------------------------------------------------------------

PhonePublisher::PhonePublisher(oap::IPhoneStateService* p, QObject* parent)
    : TopicPublisher(prodigy::api::v1::TOPIC_PHONE, parent),
      p_(p), previousCallState_(p_->callState())
{
    connect(p_, &oap::ICallStateProvider::callStateChanged, this, &PhonePublisher::onCallStateChanged);
    connect(p_, &oap::IPhoneStateService::connectionChanged, this, [this] { scheduleEmit(); });
    // ADAPTATION beyond the brief (HFP phone-seam override): capability
    // changes are status changes too -- telephonyAvailable() feeds
    // PhoneStatus.capabilities (Task 7), so its NOTIFY signal must also
    // reach subscribers.
    connect(p_, &oap::IPhoneStateService::telephonyAvailableChanged, this, [this] { scheduleEmit(); });
}

void PhonePublisher::onCallStateChanged() {
    const int state = p_->callState();
    if (state == oap::ICallStateProvider::Active &&
        previousCallState_ != oap::ICallStateProvider::Active) {
        startedAtMs_ = QDateTime::currentMSecsSinceEpoch() - qint64(p_->callDuration()) * 1000;
    } else if (state == oap::ICallStateProvider::Idle) {
        startedAtMs_ = 0;
    }
    previousCallState_ = state;
    scheduleEmit();
}

prodigy::api::v1::ApiMessage PhonePublisher::buildEnvelope() {
    prodigy::api::v1::ApiMessage m;
    m.set_request_id(0);
    *m.mutable_phone_status() = oap::api::serial::buildPhoneStatus(*p_, startedAtMs_);
    return m;
}

} // namespace oap::api
