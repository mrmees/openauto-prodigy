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
    socket_->close();
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
    socket_->close();
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
