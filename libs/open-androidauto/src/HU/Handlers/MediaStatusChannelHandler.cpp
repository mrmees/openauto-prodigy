#include "oaa/HU/Handlers/MediaStatusChannelHandler.hpp"

#include <QDebug>

#include "MediaPlaybackStatusMessage.pb.h"
#include "MediaPlaybackMetadataMessage.pb.h"

namespace oaa {
namespace hu {

MediaStatusChannelHandler::MediaStatusChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void MediaStatusChannelHandler::onChannelOpened()
{
    qInfo() << "[MediaStatusChannel] opened";
}

void MediaStatusChannelHandler::onChannelClosed()
{
    qInfo() << "[MediaStatusChannel] closed";
}

void MediaStatusChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::MediaStatusMessageId::PLAYBACK_STATUS:
        handlePlaybackStatus(payload);
        break;
    case oaa::MediaStatusMessageId::PLAYBACK_METADATA:
        handlePlaybackMetadata(payload);
        break;
    default:
        qInfo() << "[MediaStatusChannel] unknown msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << payload.size()
                << "hex:" << payload.left(64).toHex(' ');
        break;
    }
}

void MediaStatusChannelHandler::handlePlaybackStatus(const QByteArray& payload)
{
    oaa::proto::messages::MediaPlaybackStatus msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[MediaStatusChannel] failed to parse MediaPlaybackStatus";
        return;
    }

    int state = msg.playback_state();
    int sourceId = msg.source_id();

    const char* stateStr = (state == Playing) ? "PLAYING"
                         : (state == Paused)  ? "PAUSED"
                         : (state == Stopped) ? "STOPPED"
                         : "UNKNOWN";

    qDebug() << "[MediaStatusChannel] playback:" << stateStr
             << "source:" << sourceId << "pos:" << msg.position_seconds() << "s";

    emit playbackStateChanged(state, QString::number(sourceId));
}

void MediaStatusChannelHandler::handlePlaybackMetadata(const QByteArray& payload)
{
    oaa::proto::messages::MediaPlaybackMetadata msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[MediaStatusChannel] failed to parse MediaPlaybackMetadata";
        return;
    }

    QString title = QString::fromStdString(msg.title());
    QString artist = QString::fromStdString(msg.artist());
    QString album = QString::fromStdString(msg.album());
    QByteArray albumArt;
    if (msg.has_album_art()) {
        albumArt = QByteArray(msg.album_art().data(), msg.album_art().size());
    }

    qInfo() << "[MediaStatusChannel] metadata:" << title << "—" << artist
            << "—" << album << "art:" << albumArt.size() << "bytes";

    // Log new v16.1 fields if present
    if (msg.has_is_playing())
        qInfo() << "[MediaStatusChannel]   is_playing:" << msg.is_playing();
    if (msg.has_album_art_url())
        qInfo() << "[MediaStatusChannel]   album_art_url:" << QString::fromStdString(msg.album_art_url());

    emit metadataChanged(title, artist, album, albumArt);
}

} // namespace hu
} // namespace oaa
