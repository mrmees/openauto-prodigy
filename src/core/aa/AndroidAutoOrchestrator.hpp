#pragma once

#include <QObject>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <memory>

#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/TCPTransport.hpp>

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
#include "TouchHandler.hpp"
#include "NightModeProvider.hpp"

namespace oap {

class YamlConfig;
class IAudioService;
class Configuration;
struct AudioStreamHandle;

namespace aa {

class AndroidAutoOrchestrator : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum ConnectionState {
        Disconnected = 0,
        WaitingForDevice,
        Connecting,
        Connected,
        Backgrounded
    };
    Q_ENUM(ConnectionState)

    explicit AndroidAutoOrchestrator(std::shared_ptr<oap::Configuration> config,
                                      oap::IAudioService* audioService,
                                      oap::YamlConfig* yamlConfig,
                                      QObject* parent = nullptr);
    ~AndroidAutoOrchestrator() override;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    void requestVideoFocus();
    void requestExitToCar();

    VideoDecoder* videoDecoder() { return &videoDecoder_; }
    TouchHandler* touchHandler() { return &touchHandler_; }
    oaa::hu::InputChannelHandler* inputHandler() { return &inputHandler_; }

    int connectionState() const { return state_; }
    QString statusMessage() const { return statusMessage_; }

signals:
    void connectionStateChanged();
    void statusMessageChanged();

private:
    void onNewConnection();
    void onSessionStateChanged(oaa::SessionState state);
    void onSessionDisconnected(oaa::DisconnectReason reason);

    void setState(ConnectionState state, const QString& message);
    void startConnectionWatchdog();
    void stopConnectionWatchdog();
    void teardownSession();

    std::shared_ptr<oap::Configuration> config_;
    oap::IAudioService* audioService_;
    oap::YamlConfig* yamlConfig_;

    // TCP listener (Qt-native, replaces ASIO acceptor)
    QTcpServer tcpServer_;

    // Session (created per-connection)
    oaa::AASession* session_ = nullptr;
    oaa::TCPTransport* transport_ = nullptr;
    QTcpSocket* activeSocket_ = nullptr;

    // Channel handlers (from open-androidauto library)
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

    ConnectionState state_ = Disconnected;
    QString statusMessage_;

#ifdef HAS_BLUETOOTH
    class BluetoothDiscoveryService* btDiscovery_ = nullptr;
#endif
};

} // namespace aa
} // namespace oap
