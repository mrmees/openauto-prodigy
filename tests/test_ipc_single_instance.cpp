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

    static QList<QByteArray> readResponseFrames(QLocalSocket& socket, int expectedCount)
    {
        QList<QByteArray> frames;
        QByteArray buffer;
        QElapsedTimer timer;
        timer.start();
        while (frames.size() < expectedCount && timer.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            socket.waitForReadyRead(20);
            buffer += socket.readAll();
            qsizetype newline = -1;
            while ((newline = buffer.indexOf('\n')) >= 0) {
                frames.append(buffer.left(newline));
                buffer.remove(0, newline + 1);
            }
        }
        return frames;
    }

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
    void splitAndCoalescedFramesAreProcessedInOrder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.path() + QStringLiteral("/ipc.sock");
        IpcServer server;
        QVERIFY(server.start(socketPath));

        QLocalSocket socket;
        socket.connectToServer(socketPath);
        QVERIFY(socket.waitForConnected(2000));

        const QByteArray firstHalf = "{\"command\":\"sta";
        QCOMPARE(socket.write(firstHalf), qint64(firstHalf.size()));
        socket.flush();
        QTest::qWait(20);
        QCOMPARE(socket.bytesAvailable(), qint64(0));

        const QByteArray remainder =
            "tus\"}\n{\"command\":\"not_a_command\"}\n";
        QCOMPARE(socket.write(remainder), qint64(remainder.size()));
        socket.flush();

        const QList<QByteArray> responses = readResponseFrames(socket, 2);
        QCOMPARE(responses.size(), 2);
        const QJsonObject status = QJsonDocument::fromJson(responses.at(0)).object();
        QVERIFY(status.contains(QStringLiteral("version")));
        const QJsonObject unknown = QJsonDocument::fromJson(responses.at(1)).object();
        QCOMPARE(unknown.value(QStringLiteral("error")).toString(),
                 QStringLiteral("Unknown command"));
    }

    void disconnectedPartialTailDoesNotAffectNextClient()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.path() + QStringLiteral("/ipc.sock");
        IpcServer server;
        QVERIFY(server.start(socketPath));

        QLocalSocket partial;
        partial.connectToServer(socketPath);
        QVERIFY(partial.waitForConnected(2000));
        partial.write("{\"command\":\"sta");
        partial.flush();
        QTest::qWait(20);
        partial.disconnectFromServer();
        if (partial.state() != QLocalSocket::UnconnectedState)
            QVERIFY(partial.waitForDisconnected(2000));
        QCoreApplication::processEvents();

        const QJsonObject response = roundTrip(socketPath);
        QVERIFY(response.contains(QStringLiteral("version")));
    }

    void oversizedPartialFrameClosesOnlyThatClient()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.path() + QStringLiteral("/ipc.sock");
        IpcServer server;
        QVERIFY(server.start(socketPath));

        QLocalSocket oversized;
        oversized.connectToServer(socketPath);
        QVERIFY(oversized.waitForConnected(2000));
        const QByteArray payload(1024 * 1024 + 1, 'x');
        QCOMPARE(oversized.write(payload), qint64(payload.size()));
        oversized.flush();
        QTRY_COMPARE_WITH_TIMEOUT(oversized.state(), QLocalSocket::UnconnectedState, 5000);

        const QJsonObject response = roundTrip(socketPath);
        QVERIFY(response.contains(QStringLiteral("version")));
    }

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
