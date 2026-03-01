#include "oaa/HU/Handlers/MediaStatusChannelHandler.hpp"

#include <QDebug>

#include "oaa/media/MediaPlaybackStatusMessage.pb.h"
#include "oaa/media/MediaPlaybackMetadataMessage.pb.h"

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

void MediaStatusChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
{
    // Zero-copy view of data past the message ID header
    const QByteArray data = QByteArray::fromRawData(
        payload.constData() + dataOffset, payload.size() - dataOffset);

    switch (messageId) {
    case oaa::MediaStatusMessageId::PLAYBACK_STATUS:
        handlePlaybackStatus(data);
        break;
    case oaa::MediaStatusMessageId::PLAYBACK_METADATA:
        handlePlaybackMetadata(data);
        break;
    default:
        qInfo() << "[MediaStatusChannel] unknown msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << data.size()
                << "hex:" << data.left(64).toHex(' ');
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
    QString sourceApp = QString::fromStdString(msg.source_app());

    const char* stateStr = (state == Playing) ? "PLAYING"
                         : (state == Paused)  ? "PAUSED"
                         : (state == Stopped) ? "STOPPED"
                         : "UNKNOWN";

    qDebug() << "[MediaStatusChannel] playback:" << stateStr
             << "source:" << sourceApp << "pos:" << msg.position_seconds() << "s";

    emit playbackStateChanged(state, sourceApp);
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
