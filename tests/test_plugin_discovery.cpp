#include <QtTest>
#include <QDir>
#include <QFile>
#include "core/plugin/PluginDiscovery.hpp"

class TestPluginDiscovery : public QObject {
    Q_OBJECT
private slots:
    void testDiscoverFindsPlugins();
    void testDiscoverSkipsInvalid();
    void testDiscoverEmptyDir();
    void testDiscoverNonexistentDir();
    void testValidateManifestExactMatch_data();
    void testValidateManifestExactMatch();
    void testValidateManifestDefaultHost();
};

void TestPluginDiscovery::testDiscoverFindsPlugins()
{
    // Set up a temp plugins directory with one valid plugin
    QString tmpDir = QDir::tempPath() + "/oap_test_discovery";
    QDir().mkpath(tmpDir + "/test-plugin");

    QFile f(tmpDir + "/test-plugin/plugin.yaml");
    f.open(QIODevice::WriteOnly);
    f.write("id: org.test.disco\nname: Disco\nversion: '1.0'\napi_version: 3\n");
    f.close();

    oap::PluginDiscovery discovery;
    auto results = discovery.discover(tmpDir);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].id, QString("org.test.disco"));

    QDir(tmpDir).removeRecursively();
}

void TestPluginDiscovery::testDiscoverSkipsInvalid()
{
    QString tmpDir = QDir::tempPath() + "/oap_test_discovery2";
    QDir().mkpath(tmpDir + "/bad-plugin");

    // Write an invalid manifest (missing required fields)
    QFile f(tmpDir + "/bad-plugin/plugin.yaml");
    f.open(QIODevice::WriteOnly);
    f.write("name: incomplete\n");
    f.close();

    oap::PluginDiscovery discovery;
    auto results = discovery.discover(tmpDir);
    QCOMPARE(results.size(), 0);

    QDir(tmpDir).removeRecursively();
}

void TestPluginDiscovery::testDiscoverEmptyDir()
{
    QString tmpDir = QDir::tempPath() + "/oap_test_discovery3";
    QDir().mkpath(tmpDir);

    oap::PluginDiscovery discovery;
    auto results = discovery.discover(tmpDir);
    QCOMPARE(results.size(), 0);

    QDir(tmpDir).removeRecursively();
}

void TestPluginDiscovery::testDiscoverNonexistentDir()
{
    oap::PluginDiscovery discovery;
    auto results = discovery.discover("/nonexistent/path");
    QCOMPARE(results.size(), 0);
}

void TestPluginDiscovery::testValidateManifestExactMatch_data()
{
    QTest::addColumn<int>("manifestApiVersion");
    QTest::addColumn<int>("hostApiVersion");
    QTest::addColumn<bool>("expected");

    // The C++ plugin ABI has no cross-version vtable compatibility, so
    // acceptance is exact-match. A stale v1 .so built against the pre-B2
    // IHostContext vtable (companionListenerService() removed) must be
    // rejected, not accepted and mis-dispatched.
    QTest::newRow("v1 plugin, v2 host -> rejected") << 1 << 2 << false;
    QTest::newRow("v2 plugin, v2 host -> accepted") << 2 << 2 << true;
    QTest::newRow("v3 plugin, v2 host -> rejected") << 3 << 2 << false;
}

void TestPluginDiscovery::testValidateManifestExactMatch()
{
    QFETCH(int, manifestApiVersion);
    QFETCH(int, hostApiVersion);
    QFETCH(bool, expected);

    oap::PluginManifest m;
    m.id = "test";
    m.name = "Test";
    m.version = "1.0";
    m.apiVersion = manifestApiVersion;

    QCOMPARE(oap::PluginDiscovery::validateManifest(m, hostApiVersion), expected);
}

void TestPluginDiscovery::testValidateManifestDefaultHost()
{
    // Lock the real compile-time default: a silent HOST_API_VERSION bump or a
    // regression back to <=-style (forward-compatible) acceptance trips here.
    QCOMPARE(oap::PluginDiscovery::HOST_API_VERSION, 3);

    oap::PluginManifest m;
    m.id = "test";
    m.name = "Test";
    m.version = "1.0";

    // Exercises the default hostApiVersion argument (== HOST_API_VERSION).
    m.apiVersion = 3;
    QVERIFY(oap::PluginDiscovery::validateManifest(m));   // exact match -> accepted
    m.apiVersion = 2;
    QVERIFY(!oap::PluginDiscovery::validateManifest(m));  // stale v2 .so -> rejected (catches <= revert)
    m.apiVersion = 4;
    QVERIFY(!oap::PluginDiscovery::validateManifest(m));  // too new -> rejected
}

QTEST_MAIN(TestPluginDiscovery)
#include "test_plugin_discovery.moc"
