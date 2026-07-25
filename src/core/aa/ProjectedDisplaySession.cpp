#include "ProjectedDisplaySession.hpp"

#include "../Logging.hpp"
#include "../YamlConfig.hpp"

#include <QThread>
#include <QVideoFrame>

namespace oap::aa {

namespace {

oaa::proto::enums::VideoFocusMode::Enum focusModeFor(
    ProjectedSetupFocus focus)
{
    return focus == ProjectedSetupFocus::Projected
        ? oaa::proto::enums::VideoFocusMode::PROJECTED
        : oaa::proto::enums::VideoFocusMode::PROJECTED_NO_INPUT_FOCUS;
}

QString roleName(ProjectedDisplayRole role)
{
    return role == ProjectedDisplayRole::Main
        ? QStringLiteral("MAIN")
        : QStringLiteral("CLUSTER");
}

} // namespace

ProjectedDisplaySession::ProjectedDisplaySession(
    ProjectedDisplayRole role,
    uint8_t displayId,
    uint8_t videoChannelId,
    uint8_t inputChannelId,
    bool enabled,
    ProjectedSetupFocus setupFocus,
    oap::YamlConfig* yamlConfig,
    QObject* parent)
    : QObject(parent)
    , role_(role)
    , displayId_(displayId)
    , videoChannelId_(videoChannelId)
    , inputChannelId_(inputChannelId)
    , enabled_(enabled)
    , diagnosticPrefix_(QStringLiteral("%1[id=%2,ch=%3]")
                            .arg(roleName(role))
                            .arg(displayId)
                            .arg(videoChannelId))
    , videoHandler_(videoChannelId, focusModeFor(setupFocus))
    , inputHandler_(inputChannelId)
{
    if (enabled_) {
        decoder_ = std::make_unique<VideoDecoder>();
        decoder_->setYamlConfig(yamlConfig);
        decoder_->setDiagnosticLabel(diagnosticPrefix_);
        state_ = Disconnected;
        statusText_ = QStringLiteral("Android Auto disconnected");
    } else {
        state_ = Disabled;
        statusText_ = QStringLiteral("Projected display disabled");
    }

    qRegisterMetaType<std::shared_ptr<const QByteArray>>();

    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::setupRequested,
            this, [this](int codec) {
                if (!protocolActive_ || terminalStateLatched_)
                    return;
                qCInfo(lcAA).noquote() << diagnosticPrefix_
                                      << "setup requested codec=" << codec;
            });
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::handlerError,
            this, [this](const QString& message) {
                if (!protocolActive_ || terminalStateLatched_)
                    return;
                qCWarning(lcAA).noquote() << diagnosticPrefix_ << message;
            });
    connect(&inputHandler_, &oaa::hu::InputChannelHandler::handlerError,
            this, [this](const QString& message) {
                if (!protocolActive_ || terminalStateLatched_)
                    return;
                qCWarning(lcAA).noquote() << diagnosticPrefix_
                                         << "input:" << message;
            });
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::streamStarted,
            this, [this](int32_t session, uint32_t configIndex) {
                if (!protocolActive_) {
                    qCWarning(lcAA).noquote() << diagnosticPrefix_
                                             << "ignoring stream start outside protocol session";
                    return;
                }
                if (terminalStateLatched_ || !decoder_)
                    return;
                activeDecoderGeneration_ = decoder_->beginStream();
                firstDecodedLogged_ = false;
                qCInfo(lcAA).noquote() << diagnosticPrefix_
                                      << "stream started session=" << session
                                      << "config=" << configIndex
                                      << "decoder_generation="
                                      << activeDecoderGeneration_;
                if (activeDecoderGeneration_ == 0) {
                    enterTerminalState(
                        Error, QStringLiteral("decoder worker unavailable"));
                    return;
                }
                setState(WaitingForFrames,
                         QStringLiteral("Waiting for projected frames"));
            });
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::streamStopped,
            this, [this]() {
                if (!protocolActive_ || terminalStateLatched_ || !decoder_)
                    return;
                qCInfo(lcAA).noquote() << diagnosticPrefix_ << "stream stopped";
                activeDecoderGeneration_ = 0;
                decoder_->endStream();
                setState(videoChannelOpen_ ? WaitingForFrames
                                           : WaitingForChannel,
                         videoChannelOpen_
                             ? QStringLiteral("Waiting for projected frames")
                             : QStringLiteral("Waiting for projected channel"));
            });
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::videoFocusChanged,
            this, [this](int mode, bool unrequested) {
                if (!protocolActive_ || terminalStateLatched_)
                    return;
                focusMode_ = mode;
                qCInfo(lcAA).noquote() << diagnosticPrefix_
                                      << "focus mode=" << mode
                                      << "unrequested=" << unrequested;
            });
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::videoFrameData,
            this,
            [this](std::shared_ptr<const QByteArray> data,
                   qint64 enqueueTimeNs) {
                // AASession, the channel handler, and this display session
                // share one Qt event-loop thread. Direct delivery avoids a
                // frame copy but must remain owner-thread-only because this
                // path updates generation and summary state.
                Q_ASSERT(QThread::currentThread() == thread());
                if (!protocolActive_ || terminalStateLatched_ || !decoder_
                    || activeDecoderGeneration_ == 0) {
                    return;
                }
                if (!firstMediaLogged_) {
                    firstMediaLogged_ = true;
                    qCInfo(lcAA).noquote() << diagnosticPrefix_
                                          << "first media frame bytes="
                                          << (data ? data->size() : 0);
                }
                maybeLogFrameSummary();
                decoder_->decodeFrame(std::move(data), enqueueTimeNs);
            },
            Qt::DirectConnection);
    if (!decoder_)
        return;

    connect(decoder_.get(), &VideoDecoder::frameReadyForGeneration,
            this, [this](quint64 generation) {
                if (!protocolActive_ || terminalStateLatched_ || generation == 0
                    || generation != activeDecoderGeneration_) {
                    return;
                }

                QVideoFrame frame = decoder_->takeLatestFrame();
                if (frame.isValid()) {
                    if (role_ == ProjectedDisplayRole::Cluster
                        && (frame.width()
                                != kClusterViewportGeometry.encodedWidth
                            || frame.height()
                                != kClusterViewportGeometry.encodedHeight)) {
                        qCWarning(lcAA).noquote()
                            << diagnosticPrefix_
                            << "decoded frame geometry mismatch expected="
                            << kClusterViewportGeometry.encodedWidth << "x"
                            << kClusterViewportGeometry.encodedHeight
                            << "actual=" << frame.width() << "x"
                            << frame.height();
                        enterTerminalState(
                            Error,
                            QStringLiteral(
                                "Projected frame geometry mismatch"));
                        return;
                    }
                    if (!firstDecodedLogged_) {
                        firstDecodedLogged_ = true;
                        qCInfo(lcAA).noquote()
                            << diagnosticPrefix_ << "first decoded frame"
                            << frame.width() << "x" << frame.height();
                    }
                    if (QVideoSink* sink = decoder_->videoSink())
                        sink->setVideoFrame(frame);
                }
                setState(Rendering, QStringLiteral("Rendering projected display"));
            });
    connect(decoder_.get(), &VideoDecoder::streamError,
            this, [this](quint64 generation, const QString& message) {
                if (!protocolActive_ || terminalStateLatched_ || generation == 0
                    || generation != activeDecoderGeneration_) {
                    return;
                }
                qCWarning(lcAA).noquote() << diagnosticPrefix_
                                         << "decoder error:" << message;
                enterTerminalState(Error, message);
            });
    connect(decoder_.get(), &VideoDecoder::streamEnded,
            this, [this](quint64 generation) {
                qCInfo(lcAA).noquote() << diagnosticPrefix_
                                      << "decoder ended generation="
                                      << generation;
            });
}

ProjectedDisplaySession::~ProjectedDisplaySession()
{
    protocolActive_ = false;
    activeDecoderGeneration_ = 0;
    if (decoder_) {
        decoder_->endStream();
        if (decoder_->videoSink())
            decoder_->setVideoSink(nullptr);
    }
    disconnect(sinkDestroyedConnection_);
    qCInfo(lcAA).noquote() << diagnosticPrefix_ << "display session destroyed";
}

int ProjectedDisplaySession::viewportEncodedWidth() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.encodedWidth : 0;
}

int ProjectedDisplaySession::viewportEncodedHeight() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.encodedHeight : 0;
}

int ProjectedDisplaySession::viewportContentX() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.contentX() : 0;
}

int ProjectedDisplaySession::viewportContentY() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.contentY() : 0;
}

int ProjectedDisplaySession::viewportContentWidth() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.contentWidth : 0;
}

int ProjectedDisplaySession::viewportContentHeight() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.contentHeight : 0;
}

void ProjectedDisplaySession::setAdvertisedVideoConfigCount(uint32_t count)
{
    videoHandler_.setNumVideoConfigs(count);
}

void ProjectedDisplaySession::beginProtocolSession()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!enabled_)
        return;

    ++protocolGeneration_;
    protocolActive_ = true;
    terminalStateLatched_ = false;
    videoChannelOpen_ = false;
    firstMediaLogged_ = false;
    firstDecodedLogged_ = false;
    activeDecoderGeneration_ = 0;
    summaryTimer_.start();
    qCInfo(lcAA).noquote() << diagnosticPrefix_
                          << "protocol begin generation=" << protocolGeneration_;
    // A prior stream error is recoverable: beginStream() retries codec setup.
    // Only construction-level worker availability is terminal here.
    if (!decoder_ || !decoder_->isAvailable()) {
        enterTerminalState(
            Error, QStringLiteral("Projected decoder unavailable"));
        return;
    }
    setState(WaitingForChannel,
             QStringLiteral("Waiting for projected channel"));
}

void ProjectedDisplaySession::endProtocolSession()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!enabled_)
        return;

    const quint64 endedProtocolGeneration = protocolGeneration_;
    protocolActive_ = false;
    terminalStateLatched_ = false;
    videoChannelOpen_ = false;
    activeDecoderGeneration_ = 0;
    if (decoder_)
        decoder_->endStream();
    qCInfo(lcAA).noquote() << diagnosticPrefix_
                          << "protocol end generation="
                          << endedProtocolGeneration;
    setState(Disconnected, QStringLiteral("Android Auto disconnected"));
}

void ProjectedDisplaySession::noteChannelOpened(uint8_t channelId)
{
    if (!enabled_ || terminalStateLatched_)
        return;
    if (!protocolActive_) {
        qCWarning(lcAA).noquote() << diagnosticPrefix_
                                 << "ignoring channel open before protocol begin"
                                 << "channel=" << channelId;
        return;
    }
    if (channelId == videoChannelId_) {
        activeDecoderGeneration_ = 0;
        if (decoder_)
            decoder_->endStream();
        videoChannelOpen_ = true;
        qCInfo(lcAA).noquote() << diagnosticPrefix_
                              << "video channel opened";
        setState(WaitingForFrames,
                 QStringLiteral("Waiting for projected frames"));
    } else if (channelId == inputChannelId_) {
        qCInfo(lcAA).noquote() << diagnosticPrefix_
                              << "input channel opened";
    }
}

void ProjectedDisplaySession::noteChannelClosed(uint8_t channelId)
{
    if (!enabled_ || !protocolActive_ || terminalStateLatched_)
        return;
    if (channelId == videoChannelId_) {
        videoChannelOpen_ = false;
        activeDecoderGeneration_ = 0;
        if (decoder_)
            decoder_->endStream();
        qCInfo(lcAA).noquote() << diagnosticPrefix_
                              << "video channel closed";
        setState(WaitingForChannel,
                 QStringLiteral("Waiting for projected channel"));
    } else if (channelId == inputChannelId_) {
        qCInfo(lcAA).noquote() << diagnosticPrefix_
                              << "input channel closed";
    }
}

void ProjectedDisplaySession::noteChannelRejected(int32_t channelId)
{
    if (!enabled_ || !protocolActive_ || terminalStateLatched_)
        return;
    if (channelId != videoChannelId_ && channelId != inputChannelId_)
        return;

    qCWarning(lcAA).noquote() << diagnosticPrefix_
                             << "channel rejected id=" << channelId;
    enterTerminalState(
        Rejected, QStringLiteral("Phone rejected projected display"));
}

bool ProjectedDisplaySession::attachVideoSink(QVideoSink* sink)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!sink || !decoder_)
        return false;
    if (sink_ == sink)
        return true;
    if (sink_) {
        if (!sinkClaimRejectionLogged_) {
            qCWarning(lcAA).noquote() << diagnosticPrefix_
                                     << "sink claim rejected; already owned";
            sinkClaimRejectionLogged_ = true;
        }
        return false;
    }

    sinkClaimRejectionLogged_ = false;
    sink_ = sink;
    QVideoSink* claimedSink = sink;
    sinkDestroyedConnection_ = connect(
        sink, &QObject::destroyed, this, [this, claimedSink]() {
            if (!decoder_ || decoder_->videoSink() != claimedSink)
                return;
            decoder_->setVideoSink(nullptr);
            sink_.clear();
            sinkClaimRejectionLogged_ = false;
            emit videoSinkAvailabilityChanged();
            qCInfo(lcAA).noquote() << diagnosticPrefix_
                                  << "sink released on destruction";
        });

    const auto rollbackClaim = [this, claimedSink]() {
        if (sink_ != claimedSink)
            return;
        disconnect(sinkDestroyedConnection_);
        if (decoder_ && decoder_->videoSink() == claimedSink)
            decoder_->setVideoSink(nullptr);
        if (sink_ != claimedSink)
            return;
        sink_.clear();
        sinkClaimRejectionLogged_ = false;
        emit videoSinkAvailabilityChanged();
    };

    decoder_->setVideoSink(sink);
    if (sink_ != claimedSink || decoder_->videoSink() != claimedSink) {
        rollbackClaim();
        qCWarning(lcAA).noquote() << diagnosticPrefix_
                                 << "sink claim changed during attachment";
        return false;
    }

    emit videoSinkAvailabilityChanged();
    if (sink_ != claimedSink || decoder_->videoSink() != claimedSink) {
        rollbackClaim();
        qCWarning(lcAA).noquote() << diagnosticPrefix_
                                 << "sink claim changed during notification";
        return false;
    }

    qCInfo(lcAA).noquote() << diagnosticPrefix_ << "sink claimed";
    return true;
}

void ProjectedDisplaySession::detachVideoSink(QVideoSink* sink)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!sink || sink_ != sink)
        return;

    disconnect(sinkDestroyedConnection_);
    decoder_->setVideoSink(nullptr);
    sink_.clear();
    sinkClaimRejectionLogged_ = false;
    emit videoSinkAvailabilityChanged();
    qCInfo(lcAA).noquote() << diagnosticPrefix_ << "sink released";
}

void ProjectedDisplaySession::setState(State state, const QString& statusText)
{
    if (state_ == state && statusText_ == statusText)
        return;
    state_ = state;
    statusText_ = statusText;
    qCInfo(lcAA).noquote() << diagnosticPrefix_ << "state="
                          << stateName(state_);
    logSummary("state");
    emit stateChanged();
}

void ProjectedDisplaySession::enterTerminalState(
    State state, const QString& statusText)
{
    terminalStateLatched_ = true;
    activeDecoderGeneration_ = 0;
    if (decoder_)
        decoder_->endStream();
    setState(state, statusText);
}

void ProjectedDisplaySession::logSummary(const char* reason)
{
    qCInfo(lcAA).noquote()
        << diagnosticPrefix_ << "summary reason=" << reason
        << "display=" << displayId_
        << "video_ch=" << videoChannelId_
        << "input_ch=" << inputChannelId_
        << "received=" << videoHandler_.receivedFrameCount()
        << "acked=" << videoHandler_.ackCount()
        << "focus=" << focusMode_
        << "protocol_generation=" << protocolGeneration_
        << "decoder_generation=" << activeDecoderGeneration_;
    summaryTimer_.restart();
}

void ProjectedDisplaySession::maybeLogFrameSummary()
{
    if (!summaryTimer_.isValid()) {
        summaryTimer_.start();
        return;
    }
    if (summaryTimer_.elapsed() >= 5000)
        logSummary("periodic");
}

QString ProjectedDisplaySession::stateName(State state) const
{
    switch (state) {
    case Disabled: return QStringLiteral("Disabled");
    case Disconnected: return QStringLiteral("Disconnected");
    case WaitingForChannel: return QStringLiteral("WaitingForChannel");
    case Rejected: return QStringLiteral("Rejected");
    case WaitingForFrames: return QStringLiteral("WaitingForFrames");
    case Rendering: return QStringLiteral("Rendering");
    case Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

} // namespace oap::aa
