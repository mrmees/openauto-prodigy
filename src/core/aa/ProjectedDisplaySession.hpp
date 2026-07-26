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
    Q_PROPERTY(int viewportEncodedWidth READ viewportEncodedWidth
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportEncodedHeight READ viewportEncodedHeight
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentX READ viewportContentX
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentY READ viewportContentY
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentWidth READ viewportContentWidth
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentHeight READ viewportContentHeight
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(QString requestedResolution READ requestedResolution
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(int requestedDpi READ requestedDpi
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(int requestedContentWidth READ requestedContentWidth
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(int requestedContentHeight READ requestedContentHeight
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(QString requestedGalVersion READ requestedGalVersion
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(bool requestedNativeTurnCardAvailable
                   READ requestedNativeTurnCardAvailable
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(quint64 profileGeneration READ profileGeneration
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(QString profileStatusText READ profileStatusText
                   NOTIFY profileDiagnosticsChanged)
    Q_PROPERTY(bool videoSinkAvailable READ isVideoSinkAvailable
                   NOTIFY videoSinkAvailabilityChanged)

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
                            const ProjectedClusterProfile& initialProfile = {},
                            QObject* parent = nullptr);
    ~ProjectedDisplaySession() override;

    int state() const { return static_cast<int>(state_); }
    bool isRendering() const { return state_ == Rendering; }
    QString statusText() const { return statusText_; }
    int viewportEncodedWidth() const;
    int viewportEncodedHeight() const;
    int viewportContentX() const;
    int viewportContentY() const;
    int viewportContentWidth() const;
    int viewportContentHeight() const;
    QString requestedResolution() const { return requestedProfile_.resolution; }
    int requestedDpi() const { return requestedProfile_.dpi; }
    int requestedContentWidth() const { return requestedProfile_.contentWidth; }
    int requestedContentHeight() const { return requestedProfile_.contentHeight; }
    QString requestedGalVersion() const
    {
        return requestedProfile_.galVersion.toString();
    }
    bool requestedNativeTurnCardAvailable() const
    {
        return requestedProfile_.nativeTurnCardAvailable;
    }
    quint64 profileGeneration() const { return profileGeneration_; }
    QString profileStatusText() const { return profileStatusText_; }
    bool isVideoSinkAvailable() const { return sink_.isNull(); }

    ProjectedDisplayRole role() const { return role_; }
    uint8_t displayId() const { return displayId_; }
    uint8_t videoChannelId() const { return videoChannelId_; }
    uint8_t inputChannelId() const { return inputChannelId_; }
    bool isEnabled() const { return enabled_; }
    const ProjectedClusterProfile& requestedClusterProfile() const
    {
        return requestedProfile_;
    }

    oaa::hu::VideoChannelHandler* videoHandler() { return &videoHandler_; }
    oaa::hu::InputChannelHandler* inputHandler() { return &inputHandler_; }
    VideoDecoder* decoder() { return decoder_.get(); }

    void setAdvertisedVideoConfigCount(uint32_t count);
    void beginProtocolSession();
    void endProtocolSession();
    void noteChannelOpened(uint8_t channelId);
    void noteChannelClosed(uint8_t channelId);
    void noteChannelRejected(int32_t channelId);
    void activateRequestedClusterProfile();

    Q_INVOKABLE bool attachVideoSink(QVideoSink* sink);
    Q_INVOKABLE void detachVideoSink(QVideoSink* sink);
    Q_INVOKABLE bool applyClusterProfile(const QVariantMap& update);
    Q_INVOKABLE bool resetClusterProfile();

signals:
    void stateChanged();
    void videoSinkAvailabilityChanged();
    void viewportGeometryChanged();
    void profileDiagnosticsChanged();
    void clusterProfileChangeRequested();

private:
    void setState(State state, const QString& statusText);
    void enterTerminalState(State state, const QString& statusText);
    void logSummary(const char* reason);
    void maybeLogFrameSummary();
    QString stateName(State state) const;

    ProjectedDisplayRole role_;
    uint8_t displayId_;
    uint8_t videoChannelId_;
    uint8_t inputChannelId_;
    bool enabled_;
    QString diagnosticPrefix_;

    oaa::hu::VideoChannelHandler videoHandler_;
    oaa::hu::InputChannelHandler inputHandler_;
    std::unique_ptr<VideoDecoder> decoder_;

    State state_ = Disabled;
    QString statusText_;
    bool protocolActive_ = false;
    bool terminalStateLatched_ = false;
    bool videoChannelOpen_ = false;
    bool firstMediaLogged_ = false;
    bool firstDecodedLogged_ = false;
    quint64 protocolGeneration_ = 0;
    quint64 activeDecoderGeneration_ = 0;
    int focusMode_ = 0;

    ProjectedClusterProfile requestedProfile_;
    ProjectedClusterProfile activeProfile_;
    quint64 profileGeneration_ = 0;
    QString profileStatusText_ = QStringLiteral("Using startup profile");

    QPointer<QVideoSink> sink_;
    QMetaObject::Connection sinkDestroyedConnection_;
    bool sinkClaimRejectionLogged_ = false;
    QElapsedTimer summaryTimer_;
};

} // namespace oap::aa
