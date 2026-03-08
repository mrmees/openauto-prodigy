#include <oaa/Transport/ReplayTransport.hpp>

namespace oaa {

ReplayTransport::ReplayTransport(QObject* parent)
    : ITransport(parent)
{
}

ReplayTransport::~ReplayTransport() = default;

void ReplayTransport::start()
{
    started_ = true;
}

void ReplayTransport::stop()
{
    started_ = false;
}

void ReplayTransport::write(const QByteArray& data)
{
    written_.append(data);
}

bool ReplayTransport::isConnected() const
{
    return connected_;
}

void ReplayTransport::feedData(const QByteArray& data)
{
    emit dataReceived(data);
}

void ReplayTransport::simulateConnect()
{
    connected_ = true;
    emit connected();
}

void ReplayTransport::simulateDisconnect()
{
    connected_ = false;
    emit disconnected();
}

QList<QByteArray> ReplayTransport::writtenData() const
{
    return written_;
}

void ReplayTransport::clearWritten()
{
    written_.clear();
}

} // namespace oaa
