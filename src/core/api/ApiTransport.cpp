#include "core/api/ApiTransport.hpp"

#include <QTcpSocket>
#include <QWebSocket>

namespace oap::api {

// --- TcpApiTransport ---------------------------------------------------

TcpApiTransport::TcpApiTransport(QTcpSocket* socket, quint32 maxFrameBytes, QObject* parent)
    : IApiTransport(parent), socket_(socket), framer_(maxFrameBytes) {
    socket_->setParent(this);
    connect(socket_, &QTcpSocket::readyRead, this, &TcpApiTransport::onReadyRead);
    connect(socket_, &QTcpSocket::disconnected, this, &TcpApiTransport::onDisconnected);
}

void TcpApiTransport::sendMessage(const QByteArray& serialized) {
    socket_->write(ApiFramer::encode(serialized));
}

qint64 TcpApiTransport::bytesToWrite() const {
    return socket_->bytesToWrite();
}

void TcpApiTransport::close() {
    // Terminal frames (Error/AuthReject) are written in the SAME event-loop
    // turn as this close, and the session's deleteLater destroys this socket
    // on the next turn — before the loop would flush Qt's write buffer.
    // A destroyed socket aborts, discarding the frame: a real client saw
    // EOF with zero bytes (companion live bench, 2026-07-13). flush() pushes
    // the pending bytes into the kernel now; the kernel delivers them and
    // FINs even if the object dies immediately after.
    socket_->flush();
    socket_->close();
}

void TcpApiTransport::abort() {
    socket_->abort();   // discard queued data, RST — the peer learns NOW
    emit closed();      // QTcpSocket::abort() does not emit disconnected()
}

QHostAddress TcpApiTransport::peerAddress() const {
    return socket_->peerAddress();
}

void TcpApiTransport::onReadyRead() {
    const QList<QByteArray> frames = framer_.feed(socket_->readAll());
    for (const QByteArray& frame : frames) {
        emit messageReceived(frame);
    }
    if (framer_.violated()) {
        close();
    }
}

void TcpApiTransport::onDisconnected() {
    emit closed();
}

// --- WsApiTransport ------------------------------------------------------

WsApiTransport::WsApiTransport(QWebSocket* socket, quint32 maxFrameBytes, QObject* parent)
    : IApiTransport(parent), socket_(socket), maxFrameBytes_(maxFrameBytes) {
    socket_->setParent(this);
    // Reject oversized messages at the WebSocket protocol level, before Qt
    // buffers the full message -- symmetric with the TCP side, where
    // ApiFramer rejects from the 4-byte length prefix before reading the
    // body. The size check in onBinaryMessageReceived() below stays as
    // defense in depth.
    socket_->setMaxAllowedIncomingMessageSize(maxFrameBytes_);
    connect(socket_, &QWebSocket::binaryMessageReceived,
            this, &WsApiTransport::onBinaryMessageReceived);
    connect(socket_, &QWebSocket::textMessageReceived,
            this, &WsApiTransport::onTextMessageReceived);
    connect(socket_, &QWebSocket::disconnected, this, &WsApiTransport::onDisconnected);
}

void WsApiTransport::sendMessage(const QByteArray& serialized) {
    socket_->sendBinaryMessage(serialized);
}

qint64 WsApiTransport::bytesToWrite() const {
    return socket_->bytesToWrite();
}

void WsApiTransport::close() {
    // Same hazard class as the TCP transport (see above): make sure queued
    // binary frames reach the kernel before the deferred delete can destroy
    // the socket. QWebSocket::close() also flushes internally after writing
    // its close frame; this is defense in depth for the data frames.
    socket_->flush();
    socket_->close();
}

void WsApiTransport::abort() {
    socket_->abort();
    emit closed();      // QWebSocket::abort() does not emit disconnected()
}

QHostAddress WsApiTransport::peerAddress() const {
    return socket_->peerAddress();
}

void WsApiTransport::onBinaryMessageReceived(const QByteArray& message) {
    if (quint32(message.size()) > maxFrameBytes_) {
        close();
        return;
    }
    emit messageReceived(message);
}

void WsApiTransport::onTextMessageReceived(const QString&) {
    // Text frames are not a valid frame type for this protocol.
    close();
}

void WsApiTransport::onDisconnected() {
    emit closed();
}

} // namespace oap::api
