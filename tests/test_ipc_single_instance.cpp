#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

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

    static bool launchOwner(QProcess& process, const QString& socketPath)
    {
        process.start(QCoreApplication::applicationFilePath(),
                      {QStringLiteral("--ipc-owner"), socketPath});
        if (!process.waitForStarted(2000))
            return false;

        QByteArray output;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 3000) {
            if (process.waitForReadyRead(100))
                output += process.readAllStandardOutput();
            if (output.contains("READY\n"))
                return true;
            if (process.state() == QProcess::NotRunning)
                return false;
        }
        return false;
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

        QProcess first;
        QVERIFY2(launchOwner(first, socketPath), qPrintable(first.readAllStandardError()));
        QVERIFY(!roundTrip(socketPath).isEmpty());

        QProcess second;
        second.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--ipc-contender"), socketPath});
        QVERIFY(second.waitForStarted(2000));
        QVERIFY(second.waitForFinished(3000));
        QCOMPARE(second.exitStatus(), QProcess::NormalExit);
        QCOMPARE(second.exitCode(), 73);

        // The refusal must leave both the pathname and original listener live.
        QVERIFY(QFile::exists(socketPath));
        const QJsonObject response = roundTrip(socketPath);
        QVERIFY(!response.isEmpty());
        QVERIFY(response.contains(QStringLiteral("version")));

        first.kill();
        QVERIFY(first.waitForFinished(2000));
    }

    void sigkillArtifactsAreRecoveredByANewProcess()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.path() + QStringLiteral("/ipc.sock");
        const QString lockPath = socketPath + QStringLiteral(".lock");

        QProcess first;
        QVERIFY2(launchOwner(first, socketPath), qPrintable(first.readAllStandardError()));
        QVERIFY(!roundTrip(socketPath).isEmpty());
        first.kill();
        QVERIFY(first.waitForFinished(2000));
        QVERIFY(QFile::exists(socketPath));
        QVERIFY(QFile::exists(lockPath));

        QProcess recovered;
        QVERIFY2(launchOwner(recovered, socketPath),
                 qPrintable(recovered.readAllStandardError()));
        const QJsonObject response = roundTrip(socketPath);
        QVERIFY(!response.isEmpty());
        QVERIFY(response.contains(QStringLiteral("version")));
        recovered.kill();
        QVERIFY(recovered.waitForFinished(2000));
    }
};

static int runHelper(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString mode = QString::fromLocal8Bit(argv[1]);
    const QString socketPath = QString::fromLocal8Bit(argv[2]);
    IpcServer server;

    if (mode == QLatin1String("--ipc-contender"))
        return server.acquireOwnership(socketPath) ? 0 : 73;

    if (!server.acquireOwnership(socketPath) || !server.startListening())
        return 74;
    QTextStream(stdout) << "READY\n" << Qt::flush;
    return app.exec();
}

int main(int argc, char** argv)
{
    if (argc == 3
        && (QString::fromLocal8Bit(argv[1]) == QLatin1String("--ipc-owner")
            || QString::fromLocal8Bit(argv[1]) == QLatin1String("--ipc-contender"))) {
        return runHelper(argc, argv);
    }

    QCoreApplication app(argc, argv);
    TestIpcSingleInstance test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ipc_single_instance.moc"
