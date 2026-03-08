#pragma once

#include <oaa/Transport/ITransport.hpp>
#include <QHostAddress>
#include <QTcpSocket>

namespace oaa {

class TCPTransport : public ITransport {
    Q_OBJECT
public:
    explicit TCPTransport(QObject* parent = nullptr);
    ~TCPTransport() override;

    void connectToHost(const QHostAddress& address, quint16 port);
    void setSocket(QTcpSocket* socket);

    void start() override;
    void stop() override;
    void write(const QByteArray& data) override;
    bool isConnected() const override;

private:
    void connectSocketSignals();
    void disconnectSocketSignals();

    QTcpSocket* socket_ = nullptr;
    bool ownsSocket_ = true;
};

} // namespace oaa
