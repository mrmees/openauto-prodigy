#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/YamlConfig.hpp"
#include "core/aa/GalVersionPolicy.hpp"

class TestGalVersionPolicy : public QObject {
    Q_OBJECT

private slots:
    void acceptsOnlyHardwareAcceptedExactStrings()
    {
        oaa::ProtocolVersion parsed{9, 9};
        QVERIFY(oap::aa::parseSupportedGalVersion(QStringLiteral("1.7"),
                                                  &parsed));
        QCOMPARE(parsed, oaa::kGalVersion1_7);
        QVERIFY(oap::aa::parseSupportedGalVersion(QStringLiteral("4.3"),
                                                  &parsed));
        QCOMPARE(parsed, oaa::kGalVersion4_3);
        QVERIFY(oap::aa::parseSupportedGalVersion(QStringLiteral("5.0"),
                                                  &parsed));
        QCOMPARE(parsed, oaa::kGalVersion5_0);
        QVERIFY(oap::aa::parseSupportedGalVersion(QStringLiteral("5.1"),
                                                  &parsed));
        QCOMPARE(parsed, oaa::kGalVersion5_1);
        QVERIFY(oap::aa::parseSupportedGalVersion(QStringLiteral("6.0"),
                                                  &parsed));
        QCOMPARE(parsed, oaa::kGalVersion6_0);

        const QStringList rejected{
            QString(), QStringLiteral(" 4.3"), QStringLiteral("4.3 "),
            QStringLiteral("4.03"),
            QStringLiteral("6.1")};
        for (const QString& text : rejected) {
            parsed = {9, 9};
            QVERIFY2(!oap::aa::parseSupportedGalVersion(text, &parsed),
                     qPrintable(text));
            QCOMPARE(parsed, (oaa::ProtocolVersion{9, 9}));
        }
        QVERIFY(!oap::aa::parseSupportedGalVersion(QStringLiteral("4.3"),
                                                   nullptr));
    }

    void formatsNumericComponentsWithoutFloatingPoint()
    {
        QCOMPARE(oap::aa::galVersionToString({1, 10}),
                 QStringLiteral("1.10"));
        QCOMPARE(oap::aa::galVersionToString(oaa::kGalVersion4_3),
                 QStringLiteral("4.3"));
    }

    void productionAllowlistStopsAtHighestAcceptedVersion()
    {
        QCOMPARE(oap::aa::kHighestAcceptedGalVersion,
                 oaa::kGalVersion6_0);
        QCOMPARE(oap::aa::supportedGalVersionStrings(),
                 QStringList({QStringLiteral("1.7"),
                              QStringLiteral("4.3"),
                              QStringLiteral("5.0"),
                              QStringLiteral("5.1"),
                              QStringLiteral("6.0")}));
    }

    void missingPersistedSettingUsesHighestAcceptedVersion()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("legacy.yaml"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("display:\n  brightness: 42\n");
        file.close();

        oap::YamlConfig config;
        config.load(path);
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion6_0);
    }

    void invalidOrFutureSettingUsesHighestAcceptedVersion()
    {
        oap::YamlConfig config;
        QVERIFY(config.setValueByPath(QStringLiteral("connection.gal_version"),
                                      QStringLiteral("6.1")));
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion6_0);

        QVERIFY(config.setValueByPath(QStringLiteral("connection.gal_version"),
                                      QStringLiteral("not-a-version")));
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion6_0);
    }

    void explicitLegacyDowngradeRemainsPinned()
    {
        oap::YamlConfig config;
        QVERIFY(config.setValueByPath(QStringLiteral("connection.gal_version"),
                                      QStringLiteral("1.7")));
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion1_7);

        QVERIFY(config.setValueByPath(QStringLiteral("connection.gal_version"),
                                      QStringLiteral("4.3")));
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion4_3);

        QVERIFY(config.setValueByPath(QStringLiteral("connection.gal_version"),
                                      QStringLiteral("5.0")));
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion5_0);

        QVERIFY(config.setValueByPath(QStringLiteral("connection.gal_version"),
                                      QStringLiteral("5.1")));
        QCOMPARE(oap::aa::resolveConfiguredGalVersion(config),
                 oaa::kGalVersion5_1);
    }
};

QTEST_MAIN(TestGalVersionPolicy)
#include "test_gal_version_policy.moc"
