#pragma once

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QtGlobal>

#include "core/api/ApiFramer.hpp"

class QTcpSocket;
class QWebSocket;

namespace oap::api {

// Transport-agnostic abstraction over a framed byte-message connection.
// Concrete implementations (TCP, WebSocket) take ownership of the underlying
// socket and translate its wire framing into whole-message signals.
class IApiTransport : public QObject {
    Q_OBJECT
public:
    explicit IApiTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~IApiTransport() override = default;

    virtual void sendMessage(const QByteArray& serialized) = 0;
    virtual qint64 bytesToWrite() const = 0;
    // Graceful: pending frames (e.g. a terminal Error) reach the wire first.
    virtual void close() = 0;
    // Punitive: drop the connection NOW, discarding anything queued — the
    // slow-consumer kill must not wait behind a buffer the peer won't drain.
    virtual void abort() = 0;
    virtual QHostAddress peerAddress() const = 0;

signals:
    void messageReceived(const QByteArray& serialized);
    void closed();
};

// Length-prefix framed transport over a QTcpSocket (via ApiFramer).
class TcpApiTransport : public IApiTransport {
    Q_OBJECT
public:
    // Takes ownership (reparents the socket).
    TcpApiTransport(QTcpSocket* socket, quint32 maxFrameBytes, QObject* parent = nullptr);

    void sendMessage(const QByteArray& serialized) override;
    qint64 bytesToWrite() const override;
    void close() override;
    void abort() override;
    QHostAddress peerAddress() const override;

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket* socket_;
    ApiFramer framer_;
};

// Frame-per-WebSocket-message transport over a QWebSocket.
class WsApiTransport : public IApiTransport {
    Q_OBJECT
public:
    WsApiTransport(QWebSocket* socket, quint32 maxFrameBytes, QObject* parent = nullptr);

    void sendMessage(const QByteArray& serialized) override;
    qint64 bytesToWrite() const override;
    void close() override;
    void abort() override;
    QHostAddress peerAddress() const override;

private slots:
    void onBinaryMessageReceived(const QByteArray& message);
    void onTextMessageReceived(const QString& message);
    void onDisconnected();

private:
    QWebSocket* socket_;
    quint32 maxFrameBytes_;
};

} // namespace oap::api
