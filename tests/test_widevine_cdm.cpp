#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/WidevineCdm.hpp"

class TestWidevineCdm : public QObject {
    Q_OBJECT

private slots:
    void resolveReturnsFirstExistingCandidate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString missing = dir.filePath("nope/libwidevinecdm.so");
        const QString present = dir.filePath("libwidevinecdm.so");
        QFile f(present);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();

        QCOMPARE(oap::resolveWidevineCdmPath({missing, present}), present);
    }

    void resolveReturnsEmptyWhenNothingExists()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(oap::resolveWidevineCdmPath({dir.filePath("a.so"), dir.filePath("b.so")}),
                 QString());
    }

    void candidatesListRpiPathsInPriorityOrder()
    {
        const QStringList c = oap::widevineCdmCandidates();
        QCOMPARE(c.size(), 2);
        QCOMPARE(c.at(0),
                 QString("/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"));
        QCOMPARE(c.at(1),
                 QString("/opt/WidevineCdm/_platform_specific/linux_arm64/libwidevinecdm.so"));
    }

    void appendOnEmptyFlagsProducesBareSwitch()
    {
        QCOMPARE(oap::appendWidevineFlag(QByteArray(), "/opt/cdm.so"),
                 QByteArray("--widevine-path=/opt/cdm.so"));
    }

    void appendOnExistingFlagsSeparatesWithSpace()
    {
        QCOMPARE(oap::appendWidevineFlag("--disable-gpu", "/opt/cdm.so"),
                 QByteArray("--disable-gpu --widevine-path=/opt/cdm.so"));
    }

    void appendWithEmptyCdmPathIsUnchanged()
    {
        QCOMPARE(oap::appendWidevineFlag("--disable-gpu", QString()),
                 QByteArray("--disable-gpu"));
    }

    void appendRespectsOperatorOverride()
    {
        const QByteArray flags = "--widevine-path=/custom/cdm.so";
        QCOMPARE(oap::appendWidevineFlag(flags, "/opt/other.so"), flags);
    }
};

QTEST_GUILESS_MAIN(TestWidevineCdm)

#include "test_widevine_cdm.moc"
