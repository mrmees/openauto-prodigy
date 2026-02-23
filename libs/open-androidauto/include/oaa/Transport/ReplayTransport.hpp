#pragma once

#include <oaa/Transport/ITransport.hpp>
#include <QList>

namespace oaa {

class ReplayTransport : public ITransport {
    Q_OBJECT
public:
    explicit ReplayTransport(QObject* parent = nullptr);
    ~ReplayTransport() override;

    // ITransport interface
    void start() override;
    void stop() override;
    void write(const QByteArray& data) override;
    bool isConnected() const override;

    // Test API
    void feedData(const QByteArray& data);
    void simulateConnect();
    void simulateDisconnect();
    QList<QByteArray> writtenData() const;
    void clearWritten();

private:
    bool started_ = false;
    bool connected_ = false;
    QList<QByteArray> written_;
};

} // namespace oaa
