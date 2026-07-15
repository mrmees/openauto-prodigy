#pragma once

#include <QObject>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <memory>

#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/TCPTransport.hpp>
#include <oaa/Messenger/ProtocolLogger.hpp>

#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
#include <oaa/HU/Handlers/AudioChannelHandler.hpp>
#include <oaa/HU/Handlers/InputChannelHandler.hpp>
#include <oaa/HU/Handlers/SensorChannelHandler.hpp>
#include <oaa/HU/Handlers/BluetoothChannelHandler.hpp>
#include <oaa/HU/Handlers/WiFiChannelHandler.hpp>
#include <oaa/HU/Handlers/AVInputChannelHandler.hpp>
#include <oaa/HU/Handlers/StubChannelHandler.hpp>
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>
#include <oaa/HU/Handlers/MediaStatusChannelHandler.hpp>
#include <oaa/HU/Handlers/PhoneStatusChannelHandler.hpp>
#include "ServiceDiscoveryBuilder.hpp"
#include "VideoDecoder.hpp"
#include "core/services/IAudioService.hpp"
#include "TouchHandler.hpp"
#include "NightModeProvider.hpp"

namespace oap {

class YamlConfig;
class IAudioService;
class AudioService;
class IEventBus;
class IThemeService;
class IConfigService;
class EqualizerService;
class EqualizerEngine;
struct AudioStreamHandle;

namespace aa {

class AndroidAutoOrchestrator : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int phoneBatteryLevel READ phoneBatteryLevel NOTIFY phoneBatteryChanged)
    Q_PROPERTY(int phoneSignalStrength READ phoneSignalStrength NOTIFY phoneSignalChanged)
    Q_PROPERTY(bool aaConnected READ isAaConnected NOTIFY connectionStateChanged)

public:
    enum ConnectionState {
        Disconnected = 0,
        WaitingForDevice,
        Connecting,
        Connected,
        Backgrounded
    };
    Q_ENUM(ConnectionState)

    explicit AndroidAutoOrchestrator(oap::IConfigService* configService,
                                      oap::IAudioService* audioService,
                                      oap::YamlConfig* yamlConfig,
                                      oap::IEventBus* eventBus = nullptr,
                                      oap::EqualizerService* eqService = nullptr,
                                      QObject* parent = nullptr);
    ~AndroidAutoOrchestrator() override;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    void disconnectSession();
    void disconnectAndRetrigger();
    void requestVideoFocus();
    void requestExitToCar();

    /// Send a button press+release via the Input channel (Android keycode)
    Q_INVOKABLE void sendButtonPress(int keycode);

    VideoDecoder* videoDecoder() { return &videoDecoder_; }
    TouchHandler* touchHandler() { return &touchHandler_; }

    /// Set theme service for applying phone-sent AA theming tokens.
    void setThemeService(oap::IThemeService* theme) { themeService_ = theme; }

    /// Set detected display dimensions for margin calculations.
    void setDisplayDimensions(int w, int h) { displayW_ = w; displayH_ = h; }

    /// Set DPI-scaled navbar thickness for margin calculations.
    void setNavbarThickness(int thickness) { navbarThickness_ = thickness; }
    oaa::hu::InputChannelHandler* inputHandler() { return &inputHandler_; }
    oaa::hu::NavigationChannelHandler* navigationHandler() { return &navHandler_; }
    oaa::hu::MediaStatusChannelHandler* mediaStatusHandler() { return &mediaStatusHandler_; }

    int connectionState() const { return state_; }
    QString statusMessage() const { return statusMessage_; }
    int phoneBatteryLevel() const { return phoneBatteryLevel_; }
    int phoneSignalStrength() const { return phoneSignalStrength_; }
    bool isAaConnected() const { return state_ == Connected || state_ == Backgrounded; }

signals:
    void connectionStateChanged();
    void statusMessageChanged();
    void phoneBatteryChanged();
    void phoneSignalChanged();

private:
    void onNewConnection();
    void onSessionStateChanged(oaa::SessionState state);
    void onSessionDisconnected(oaa::DisconnectReason reason);

    void setState(ConnectionState state, const QString& message);
    void startConnectionWatchdog();
    void stopConnectionWatchdog();
    void teardownSession();
    void startProtocolCapture();
    void stopProtocolCapture();

    oap::IConfigService* configService_;
    oap::IAudioService* audioService_;
    // Concrete AudioService (createStreamWithOptions), cached from a
    // dynamic_cast of audioService_ in the constructor; null when a mock
    // IAudioService is injected (tests) — the legacy createStream path is used
    // in that case.
    oap::AudioService* concreteAudio_ = nullptr;
    oap::YamlConfig* yamlConfig_;
    oap::IEventBus* eventBus_;
    oap::EqualizerService* eqService_;
    oap::IThemeService* themeService_ = nullptr;

    // TCP listener (Qt-native, replaces ASIO acceptor)
    QTcpServer tcpServer_;

    // Session (created per-connection)
    oaa::AASession* session_ = nullptr;
    oaa::TCPTransport* transport_ = nullptr;
    QTcpSocket* activeSocket_ = nullptr;

    // Channel handlers (from prodigy-oaa-protocol library)
    oaa::hu::VideoChannelHandler videoHandler_;
    oaa::hu::AudioChannelHandler mediaAudioHandler_{oaa::ChannelId::MediaAudio};
    oaa::hu::AudioChannelHandler speechAudioHandler_{oaa::ChannelId::SpeechAudio};
    oaa::hu::AudioChannelHandler systemAudioHandler_{oaa::ChannelId::SystemAudio};
    oaa::hu::InputChannelHandler inputHandler_;
    oaa::hu::SensorChannelHandler sensorHandler_;
    oaa::hu::BluetoothChannelHandler btHandler_;
    std::unique_ptr<oaa::hu::WiFiChannelHandler> wifiHandler_;
    oaa::hu::AVInputChannelHandler avInputHandler_;
    oaa::hu::NavigationChannelHandler navHandler_;
    oaa::hu::MediaStatusChannelHandler mediaStatusHandler_;
    oaa::hu::PhoneStatusChannelHandler phoneStatusHandler_;

    // Shared resources
    TouchHandler touchHandler_;
    VideoDecoder videoDecoder_;
    std::unique_ptr<NightModeProvider> nightProvider_;
    QTimer watchdogTimer_;

    // PipeWire audio stream handles (created/destroyed per session)
    oap::AudioStreamHandle* mediaStream_ = nullptr;
    oap::AudioStreamHandle* speechStream_ = nullptr;
    oap::AudioStreamHandle* systemStream_ = nullptr;

    // Dedicated EQ engine instances (acquired from eqService_ per session,
    // released in teardownSession() AFTER the streams are destroyed — RT
    // ordering contract §4.4). Non-owning here; the service owns them.
    oap::EqualizerEngine* mediaEq_ = nullptr;
    oap::EqualizerEngine* speechEq_ = nullptr;
    oap::EqualizerEngine* systemEq_ = nullptr;

    // How speech-channel audio treats music while active: the phone's last
    // focus request sets it (GAIN_TRANSIENT = mute, GAIN_NAVI = duck).
    oap::AudioFocusType speechFocusHint_ = oap::AudioFocusType::GainTransientMayDuck;

    // Per-stream activity, tracked from streamStarted/streamStopped. A phone
    // RELEASE must only drop focus still held by INACTIVE streams — an active
    // stream's focus is owned by streamStarted/streamStopped (bench 2026-07-09).
    bool mediaStreamActive_ = false;
    bool speechStreamActive_ = false;
    bool systemStreamActive_ = false;

    std::unique_ptr<oaa::ProtocolLogger> protocolLogger_;

    ConnectionState state_ = Disconnected;
    QString statusMessage_;
    bool pendingReconnect_ = false;
    int phoneBatteryLevel_ = -1;
    int phoneSignalStrength_ = -1;
    int displayW_ = 0;  // detected display dimensions (0 = use config fallback)
    int displayH_ = 0;
    int navbarThickness_ = 56;

#ifdef HAS_BLUETOOTH
    class BluetoothDiscoveryService* btDiscovery_ = nullptr;
#endif
};

} // namespace aa
} // namespace oap
