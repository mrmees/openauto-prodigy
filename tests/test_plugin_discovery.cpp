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
    void testValidateManifestApiVersion();
};

void TestPluginDiscovery::testDiscoverFindsPlugins()
{
    // Set up a temp plugins directory with one valid plugin
    QString tmpDir = QDir::tempPath() + "/oap_test_discovery";
    QDir().mkpath(tmpDir + "/test-plugin");

    QFile f(tmpDir + "/test-plugin/plugin.yaml");
    f.open(QIODevice::WriteOnly);
    f.write("id: org.test.disco\nname: Disco\nversion: '1.0'\napi_version: 1\n");
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

void TestPluginDiscovery::testValidateManifestApiVersion()
{
    oap::PluginManifest m;
    m.id = "test";
    m.name = "Test";
    m.version = "1.0";
    m.apiVersion = 1;

    QVERIFY(oap::PluginDiscovery::validateManifest(m, 1));
    QVERIFY(oap::PluginDiscovery::validateManifest(m, 2)); // forward compatible
    m.apiVersion = 3;
    QVERIFY(!oap::PluginDiscovery::validateManifest(m, 2)); // too new
}

QTEST_MAIN(TestPluginDiscovery)
#include "test_plugin_discovery.moc"
