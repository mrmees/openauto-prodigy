#include <QtTest>
#include "core/plugin/PluginManifest.hpp"

class TestPluginManifest : public QObject {
    Q_OBJECT
private slots:
    void testParseValidManifest();
    void testInvalidManifest();
    void testMissingFields();
    void testRequiredServices();
    void testSettingsSchema();
    void testNavStripConfig();
};

void TestPluginManifest::testParseValidManifest()
{
    auto m = oap::PluginManifest::fromFile(QString(TEST_DATA_DIR) + "/test_plugin.yaml");
    QVERIFY(m.isValid());
    QCOMPARE(m.id, QString("org.test.example"));
    QCOMPARE(m.name, QString("Test Plugin"));
    QCOMPARE(m.version, QString("1.0.0"));
    QCOMPARE(m.apiVersion, 1);
    QCOMPARE(m.type, QString("full"));
    QCOMPARE(m.description, QString("A test plugin"));
    QCOMPARE(m.author, QString("Test"));
    QCOMPARE(m.icon, QString("icons/test.svg"));
}

void TestPluginManifest::testInvalidManifest()
{
    // Non-existent file
    auto m = oap::PluginManifest::fromFile("/nonexistent/plugin.yaml");
    QVERIFY(!m.isValid());
}

void TestPluginManifest::testMissingFields()
{
    // Write a minimal (but incomplete) manifest to temp
    QString path = QDir::tempPath() + "/oap_test_bad_manifest.yaml";
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write("name: incomplete\n");
    f.close();

    auto m = oap::PluginManifest::fromFile(path);
    QVERIFY(!m.isValid()); // missing id, version, apiVersion

    QFile::remove(path);
}

void TestPluginManifest::testRequiredServices()
{
    auto m = oap::PluginManifest::fromFile(QString(TEST_DATA_DIR) + "/test_plugin.yaml");
    QCOMPARE(m.requiredServices.size(), 2);
    QCOMPARE(m.requiredServices[0], QString("AudioService"));
    QCOMPARE(m.requiredServices[1], QString("ConfigService"));
}

void TestPluginManifest::testSettingsSchema()
{
    auto m = oap::PluginManifest::fromFile(QString(TEST_DATA_DIR) + "/test_plugin.yaml");
    QCOMPARE(m.settings.size(), 2);

    QCOMPARE(m.settings[0].key, QString("enabled"));
    QCOMPARE(m.settings[0].type, QString("bool"));
    QCOMPARE(m.settings[0].defaultValue.toBool(), true);

    QCOMPARE(m.settings[1].key, QString("quality"));
    QCOMPARE(m.settings[1].type, QString("enum"));
    QCOMPARE(m.settings[1].options.size(), 3);
    QCOMPARE(m.settings[1].options[2], QString("high"));
}

void TestPluginManifest::testNavStripConfig()
{
    auto m = oap::PluginManifest::fromFile(QString(TEST_DATA_DIR) + "/test_plugin.yaml");
    QCOMPARE(m.navStripOrder, 1);
    QCOMPARE(m.navStripVisible, true);
}

QTEST_MAIN(TestPluginManifest)
#include "test_plugin_manifest.moc"
