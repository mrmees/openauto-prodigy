#pragma once

#include <QObject>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <memory>

#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/TCPTransport.hpp>

#include "handlers/VideoChannelHandler.hpp"
#include "handlers/AudioChannelHandler.hpp"
#include "handlers/InputChannelHandler.hpp"
#include "handlers/SensorChannelHandler.hpp"
#include "handlers/BluetoothChannelHandler.hpp"
#include "handlers/WiFiChannelHandler.hpp"
#include "handlers/AVInputChannelHandler.hpp"
#include "ServiceDiscoveryBuilder.hpp"
#include "VideoDecoder.hpp"
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
    InputChannelHandler* inputHandler() { return &inputHandler_; }

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

    // Channel handlers
    VideoChannelHandler videoHandler_;
    AudioChannelHandler mediaAudioHandler_{oaa::ChannelId::MediaAudio};
    AudioChannelHandler speechAudioHandler_{oaa::ChannelId::SpeechAudio};
    AudioChannelHandler systemAudioHandler_{oaa::ChannelId::SystemAudio};
    InputChannelHandler inputHandler_;
    SensorChannelHandler sensorHandler_;
    BluetoothChannelHandler btHandler_;
    std::unique_ptr<WiFiChannelHandler> wifiHandler_;
    AVInputChannelHandler avInputHandler_;

    // Shared resources
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
