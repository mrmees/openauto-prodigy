#pragma once

// Topic publishers: coalescing fan-in from provider change signals to the
// External API's server-initiated status stream. Every publisher owns a
// 0-ms single-shot QTimer that collapses any number of provider signal
// emissions within the same event-loop turn into exactly one statusReady
// emission on the next turn -- a Bluetooth metadata update immediately
// followed by a playback-state update (two provider signals, same call
// stack) reaches subscribers as one full snapshot, not two.
//
// Publishers never normalize data themselves -- buildEnvelope() always
// delegates to the Task 6/7 serializers (core/api/ApiSerializers.hpp).

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>

#include "api/api.pb.h"

namespace oap {
class IMediaStatusProvider;
class INavigationProvider;
class IProjectionStatusProvider;
class IPhoneStateService;
class ThemeService;
class BluetoothManager;
} // namespace oap

namespace oap::api {

/// Base class for the five status-topic publishers.
class TopicPublisher : public QObject {
    Q_OBJECT
public:
    TopicPublisher(prodigy::api::v1::Topic topic, QObject* parent = nullptr);

    prodigy::api::v1::Topic topic() const { return topic_; }

    /// Serializes buildEnvelope() right now (request_id 0). Used to answer a
    /// fresh SubscribeRequest with an immediate full snapshot.
    QByteArray snapshotBytes();

signals:
    void statusReady(prodigy::api::v1::Topic topic, const QByteArray& envelopeBytes);

protected:
    /// Schedules a coalesced emit on the next event-loop turn. Safe to call
    /// any number of times within the same turn -- only the first call
    /// (re)starts the single-shot timer.
    void scheduleEmit();

    virtual prodigy::api::v1::ApiMessage buildEnvelope() = 0;

private:
    void emitNow();

    prodigy::api::v1::Topic topic_;
    QTimer coalesce_;
};

class MediaPublisher : public TopicPublisher {
    Q_OBJECT
public:
    explicit MediaPublisher(oap::IMediaStatusProvider* p, QObject* parent = nullptr);

protected:
    prodigy::api::v1::ApiMessage buildEnvelope() override;

private:
    oap::IMediaStatusProvider* p_;
};

class NavigationPublisher : public TopicPublisher {
    Q_OBJECT
public:
    explicit NavigationPublisher(oap::INavigationProvider* p, QObject* parent = nullptr);

protected:
    prodigy::api::v1::ApiMessage buildEnvelope() override;

private:
    oap::INavigationProvider* p_;
};

class ProjectionPublisher : public TopicPublisher {
    Q_OBJECT
public:
    explicit ProjectionPublisher(oap::IProjectionStatusProvider* p, QObject* parent = nullptr);

protected:
    prodigy::api::v1::ApiMessage buildEnvelope() override;

private:
    oap::IProjectionStatusProvider* p_;
};

class SystemPublisher : public TopicPublisher {
    Q_OBJECT
public:
    /// `bt` is nullable -- Bluetooth stack may be unavailable on this head
    /// unit (see ApiSerializers buildSystemStatus).
    SystemPublisher(oap::ThemeService* theme, QString appVersion,
                     oap::BluetoothManager* bt, QObject* parent = nullptr);

protected:
    prodigy::api::v1::ApiMessage buildEnvelope() override;

private:
    oap::ThemeService* theme_;
    QString appVersion_;
    oap::BluetoothManager* bt_;
};

class PhonePublisher : public TopicPublisher {
    Q_OBJECT
public:
    explicit PhonePublisher(oap::IPhoneStateService* p, QObject* parent = nullptr);

protected:
    prodigy::api::v1::ApiMessage buildEnvelope() override;

private:
    void onCallStateChanged();

    oap::IPhoneStateService* p_;
    int previousCallState_;
    qint64 startedAtMs_ = 0;
};

} // namespace oap::api
