#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QVideoSink>

#include <cstdint>
#include <memory>

#include <oaa/HU/Handlers/InputChannelHandler.hpp>
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>

#include "ProjectedDisplayConfig.hpp"
#include "VideoDecoder.hpp"

namespace oap { class YamlConfig; }

namespace oap::aa {

class ProjectedDisplaySession : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool rendering READ isRendering NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

public:
    enum State {
        Disabled = 0,
        Disconnected,
        WaitingForChannel,
        Rejected,
        WaitingForFrames,
        Rendering,
        Error,
    };
    Q_ENUM(State)

    ProjectedDisplaySession(ProjectedDisplayRole role,
                            uint8_t displayId,
                            uint8_t videoChannelId,
                            uint8_t inputChannelId,
                            bool enabled,
                            ProjectedSetupFocus setupFocus,
                            oap::YamlConfig* yamlConfig,
                            QObject* parent = nullptr);
    ~ProjectedDisplaySession() override;

    int state() const { return static_cast<int>(state_); }
    bool isRendering() const { return state_ == Rendering; }
    QString statusText() const { return statusText_; }

    ProjectedDisplayRole role() const { return role_; }
    uint8_t displayId() const { return displayId_; }
    uint8_t videoChannelId() const { return videoChannelId_; }
    uint8_t inputChannelId() const { return inputChannelId_; }
    bool isEnabled() const { return enabled_; }

    oaa::hu::VideoChannelHandler* videoHandler() { return &videoHandler_; }
    oaa::hu::InputChannelHandler* inputHandler() { return &inputHandler_; }
    VideoDecoder* decoder() { return decoder_.get(); }

    void setAdvertisedVideoConfigCount(uint32_t count);
    void beginProtocolSession();
    void endProtocolSession();
    void noteChannelOpened(uint8_t channelId);
    void noteChannelRejected(int32_t channelId);

    Q_INVOKABLE bool attachVideoSink(QVideoSink* sink);
    Q_INVOKABLE void detachVideoSink(QVideoSink* sink);

signals:
    void stateChanged();

private:
    void setState(State state, const QString& statusText);
    void logSummary(const char* reason);
    void maybeLogFrameSummary();
    QString stateName(State state) const;

    ProjectedDisplayRole role_;
    uint8_t displayId_;
    uint8_t videoChannelId_;
    uint8_t inputChannelId_;
    bool enabled_;
    ProjectedSetupFocus setupFocus_;
    QString diagnosticPrefix_;

    oaa::hu::VideoChannelHandler videoHandler_;
    oaa::hu::InputChannelHandler inputHandler_;
    std::unique_ptr<VideoDecoder> decoder_;

    State state_ = Disabled;
    QString statusText_;
    bool protocolActive_ = false;
    bool videoChannelOpen_ = false;
    bool firstMediaLogged_ = false;
    bool firstDecodedLogged_ = false;
    quint64 protocolGeneration_ = 0;
    quint64 activeDecoderGeneration_ = 0;
    int focusMode_ = 0;

    QPointer<QVideoSink> sink_;
    QMetaObject::Connection sinkDestroyedConnection_;
    QElapsedTimer summaryTimer_;
};

} // namespace oap::aa
