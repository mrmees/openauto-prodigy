#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/services/IpcServer.hpp"

using namespace oap;

class TestIpcSingleInstance : public QObject {
    Q_OBJECT

    static QJsonObject roundTrip(const QString& socketPath)
    {
        QLocalSocket socket;
        socket.connectToServer(socketPath);
        if (!socket.waitForConnected(2000))
            return {};

        socket.write("{\"command\":\"status\"}\n");
        socket.flush();

        QElapsedTimer timer;
        timer.start();
        while (socket.bytesAvailable() == 0 && timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            socket.waitForReadyRead(20);
        }
        return QJsonDocument::fromJson(socket.readAll().trimmed()).object();
    }

    static void createStaleUnixSocket(const QString& socketPath)
    {
        const QByteArray nativePath = QFile::encodeName(socketPath);
        QVERIFY2(nativePath.size() < static_cast<int>(sizeof(sockaddr_un::sun_path)),
                 "temporary socket path exceeds sockaddr_un capacity");

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(fd >= 0);

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, nativePath.constData(), nativePath.size() + 1);
        const socklen_t addressLength = static_cast<socklen_t>(
            offsetof(sockaddr_un, sun_path) + nativePath.size() + 1);
        const int bindResult = ::bind(
            fd, reinterpret_cast<const sockaddr*>(&address), addressLength);
        ::close(fd);
        QVERIFY(bindResult == 0);
        QVERIFY(QFile::exists(socketPath));
    }

private slots:
    void staleRemovalRequiresExplicitNoListenerError()
    {
        QVERIFY(IpcServer::isExplicitlyStaleSocketError(
            QLocalSocket::ConnectionRefusedError));
        QVERIFY(IpcServer::isExplicitlyStaleSocketError(
            QLocalSocket::ServerNotFoundError));

        // Timeouts, permissions, and generic failures can all hide a live
        // pre-lock listener. They must fail closed and preserve its pathname.
        QVERIFY(!IpcServer::isExplicitlyStaleSocketError(
            QLocalSocket::SocketTimeoutError));
        QVERIFY(!IpcServer::isExplicitlyStaleSocketError(
            QLocalSocket::SocketAccessError));
        QVERIFY(!IpcServer::isExplicitlyStaleSocketError(
            QLocalSocket::UnknownSocketError));
        QVERIFY(!IpcServer::isExplicitlyStaleSocketError(
            QLocalSocket::OperationError));
    }

    void liveOwnerCannotBeStolen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.path() + QStringLiteral("/ipc.sock");

        IpcServer first;
        QVERIFY(first.start(socketPath));
        QVERIFY(!roundTrip(socketPath).isEmpty());

        IpcServer second;
        QVERIFY(!second.start(socketPath));

        // The refusal must leave both the pathname and original listener live.
        QVERIFY(QFile::exists(socketPath));
        const QJsonObject response = roundTrip(socketPath);
        QVERIFY(!response.isEmpty());
        QVERIFY(response.contains(QStringLiteral("version")));
    }

    void staleSocketIsRecovered()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.path() + QStringLiteral("/ipc.sock");
        createStaleUnixSocket(socketPath);

        IpcServer server;
        QVERIFY(server.start(socketPath));
        const QJsonObject response = roundTrip(socketPath);
        QVERIFY(!response.isEmpty());
        QVERIFY(response.contains(QStringLiteral("version")));
    }
};

QTEST_MAIN(TestIpcSingleInstance)
#include "test_ipc_single_instance.moc"
