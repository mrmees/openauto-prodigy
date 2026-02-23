#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace oaa {

class ITransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~ITransport() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void write(const QByteArray& data) = 0;
    virtual bool isConnected() const = 0;

signals:
    void dataReceived(const QByteArray& data);
    void connected();
    void disconnected();
    void error(const QString& message);
};

} // namespace oaa
