#include <oaa/Transport/TCPTransport.hpp>

namespace oaa {

TCPTransport::TCPTransport(QObject* parent)
    : ITransport(parent)
{
}

TCPTransport::~TCPTransport()
{
    stop();
    if (ownsSocket_) {
        delete socket_;
    }
}

void TCPTransport::connectToHost(const QHostAddress& address, quint16 port)
{
    if (!socket_) {
        socket_ = new QTcpSocket(this);
        ownsSocket_ = true;
    }
    socket_->connectToHost(address, port);
}

void TCPTransport::setSocket(QTcpSocket* socket)
{
    stop();
    if (ownsSocket_) {
        delete socket_;
    }
    socket_ = socket;
    ownsSocket_ = false;
    if (socket_) {
        socket_->setParent(this);
    }
}

void TCPTransport::start()
{
    if (!socket_) return;
    connectSocketSignals();
}

void TCPTransport::stop()
{
    if (!socket_) return;
    disconnectSocketSignals();
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }
}

void TCPTransport::write(const QByteArray& data)
{
    if (socket_ && socket_->state() == QAbstractSocket::ConnectedState) {
        socket_->write(data);
    } else {
        qWarning() << "[TCPTransport] write DROPPED:" << data.size()
                    << "bytes (socket state:" << (socket_ ? (int)socket_->state() : -1) << ")";
    }
}

bool TCPTransport::isConnected() const
{
    return socket_ && socket_->state() == QAbstractSocket::ConnectedState;
}

void TCPTransport::connectSocketSignals()
{
    connect(socket_, &QTcpSocket::readyRead, this, [this]() {
        QByteArray data = socket_->readAll();
        qDebug() << "[TCPTransport] readyRead:" << data.size() << "bytes";
        emit dataReceived(data);
    });
    connect(socket_, &QTcpSocket::connected, this, &TCPTransport::connected);
    connect(socket_, &QTcpSocket::disconnected, this, &TCPTransport::disconnected);
    connect(socket_, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit error(socket_->errorString());
    });
}

void TCPTransport::disconnectSocketSignals()
{
    disconnect(socket_, nullptr, this, nullptr);
}

} // namespace oaa
