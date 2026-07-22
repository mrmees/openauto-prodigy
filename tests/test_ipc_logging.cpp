#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "core/Logging.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/IpcServer.hpp"

using namespace oap;

class TestIpcLogging : public QObject {
    Q_OBJECT

    QString socketPath() const
    {
        return QDir::tempPath() + "/oap-ipc-logging-"
            + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sock";
    }

    QJsonObject roundTrip(const QString& socketPath, const QJsonObject& request)
    {
        QLocalSocket socket;
        socket.connectToServer(socketPath);
        if (!socket.waitForConnected(2000))
            return {};

        socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
        socket.flush();

        QElapsedTimer timer;
        timer.start();
        while (socket.bytesAvailable() == 0 && timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            socket.waitForReadyRead(20);
        }

        const QJsonObject response = QJsonDocument::fromJson(socket.readAll().trimmed()).object();
        socket.disconnectFromServer();
        return response;
    }

private slots:
    void init()
    {
        oap::setDebugCategories({});
    }

    void cleanup()
    {
        oap::setDebugCategories({});
    }

    void setLoggingPersistsVerboseAndSelectiveCategories()
    {
        QTemporaryDir directory;
        const QString configPath = directory.path() + "/config.yaml";
        const QString ipcSocketPath = socketPath();
        YamlConfig config;
        IpcServer server;
        server.setConfig(&config, configPath);
        QVERIFY(server.start(ipcSocketPath));

        const QJsonArray categories{QStringLiteral("aa"), QStringLiteral("bt")};
        const QJsonObject categoriesResponse = roundTrip(
            ipcSocketPath,
            {{"command", "set_logging"}, {"data", QJsonObject{{"categories", categories}}}});
        QVERIFY(categoriesResponse.value("ok").toBool());
        QVERIFY(!oap::isVerbose());
        QVERIFY(lcAA().isDebugEnabled());
        QVERIFY(lcBT().isDebugEnabled());
        QVERIFY(!lcAudio().isDebugEnabled());

        const QJsonObject verboseResponse = roundTrip(
            ipcSocketPath,
            {{"command", "set_logging"}, {"data", QJsonObject{{"verbose", true}}}});
        QVERIFY(verboseResponse.value("ok").toBool());
        QVERIFY(oap::isVerbose());
        QVERIFY(config.loggingVerbose());

        const QJsonObject selectiveResponse = roundTrip(
            ipcSocketPath,
            {{"command", "set_logging"}, {"data", QJsonObject{{"verbose", false}}}});
        QVERIFY(selectiveResponse.value("ok").toBool());
        QVERIFY(!oap::isVerbose());
        QVERIFY(lcAA().isDebugEnabled());
        QVERIFY(lcBT().isDebugEnabled());
        QVERIFY(!lcAudio().isDebugEnabled());

        const QJsonObject logging = roundTrip(ipcSocketPath, {{"command", "get_logging"}});
        QVERIFY(!logging.value("verbose").toBool());
        QCOMPARE(logging.value("debug_categories").toArray(), categories);

        YamlConfig reloaded;
        reloaded.load(configPath);
        QVERIFY(!reloaded.loggingVerbose());
        QCOMPARE(reloaded.loggingDebugCategories(), QStringList({"aa", "bt"}));
    }

    void setLoggingReportsPersistenceFailure()
    {
        QTemporaryDir directory;
        YamlConfig config;
        IpcServer server;
        server.setConfig(&config, directory.path() + "/missing/config.yaml");
        const QString ipcSocketPath = socketPath();
        QVERIFY(server.start(ipcSocketPath));

        const QJsonObject response = roundTrip(
            ipcSocketPath,
            {{"command", "set_logging"}, {"data", QJsonObject{{"verbose", true}}}});
        QVERIFY(!response.value("ok").toBool());
        QVERIFY(!response.value("error").toString().isEmpty());
    }

    void setLoggingReportsMissingConfig()
    {
        QTemporaryDir directory;
        IpcServer server;
        const QString ipcSocketPath = socketPath();
        QVERIFY(server.start(ipcSocketPath));

        const QJsonObject response = roundTrip(
            ipcSocketPath,
            {{"command", "set_logging"}, {"data", QJsonObject{{"verbose", true}}}});
        QVERIFY(!response.value("ok").toBool());
        QVERIFY(!response.value("error").toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestIpcLogging)
#include "test_ipc_logging.moc"
