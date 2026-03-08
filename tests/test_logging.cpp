#include <QtTest>
#include "core/Logging.hpp"

class TestLogging : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Category existence and validity
    void testCategoriesExist();
    void testDefaultThresholdIsInfo();

    // Verbose toggle
    void testSetVerboseEnablesDebug();
    void testSetVerboseFalseRestoresQuiet();

    // Selective category enabling
    void testSetDebugCategoriesSelective();
    void testSetDebugCategoriesAaEnablesLibrary();

    // Library message detection
    void testLibraryDetectionByCategory();
    void testLibraryDetectionByFilePath();
    void testLibraryDetectionByBracketTag();
    void testLibraryDetectionByColonPrefix();
    void testLibraryDetectionNewTags();
    void testNonLibraryMessage();

    void cleanupTestCase();
};

void TestLogging::initTestCase()
{
    // Install the handler so filter rules work
    oap::installLogHandler();
}

void TestLogging::testCategoriesExist()
{
    // Each category should be a valid QLoggingCategory with the expected name
    QCOMPARE(QString::fromLatin1(lcAA().categoryName()), QStringLiteral("oap.aa"));
    QCOMPARE(QString::fromLatin1(lcBT().categoryName()), QStringLiteral("oap.bt"));
    QCOMPARE(QString::fromLatin1(lcAudio().categoryName()), QStringLiteral("oap.audio"));
    QCOMPARE(QString::fromLatin1(lcPlugin().categoryName()), QStringLiteral("oap.plugin"));
    QCOMPARE(QString::fromLatin1(lcUI().categoryName()), QStringLiteral("oap.ui"));
    QCOMPARE(QString::fromLatin1(lcCore().categoryName()), QStringLiteral("oap.core"));
}

void TestLogging::testDefaultThresholdIsInfo()
{
    // Debug should be disabled by default (QtInfoMsg threshold)
    // Reset to defaults first
    oap::setVerbose(false);

    QVERIFY(!lcAA().isDebugEnabled());
    QVERIFY(!lcBT().isDebugEnabled());
    QVERIFY(!lcAudio().isDebugEnabled());
    QVERIFY(!lcPlugin().isDebugEnabled());
    QVERIFY(!lcUI().isDebugEnabled());
    QVERIFY(!lcCore().isDebugEnabled());

    // Info and above should be enabled
    QVERIFY(lcAA().isInfoEnabled());
    QVERIFY(lcAA().isWarningEnabled());
    QVERIFY(lcAA().isCriticalEnabled());
}

void TestLogging::testSetVerboseEnablesDebug()
{
    oap::setVerbose(true);
    QVERIFY(oap::isVerbose());

    QVERIFY(lcAA().isDebugEnabled());
    QVERIFY(lcBT().isDebugEnabled());
    QVERIFY(lcAudio().isDebugEnabled());
    QVERIFY(lcPlugin().isDebugEnabled());
    QVERIFY(lcUI().isDebugEnabled());
    QVERIFY(lcCore().isDebugEnabled());
}

void TestLogging::testSetVerboseFalseRestoresQuiet()
{
    oap::setVerbose(true);
    QVERIFY(lcAA().isDebugEnabled());

    oap::setVerbose(false);
    QVERIFY(!oap::isVerbose());

    QVERIFY(!lcAA().isDebugEnabled());
    QVERIFY(!lcBT().isDebugEnabled());
    QVERIFY(!lcAudio().isDebugEnabled());
    QVERIFY(!lcPlugin().isDebugEnabled());
    QVERIFY(!lcUI().isDebugEnabled());
    QVERIFY(!lcCore().isDebugEnabled());
}

void TestLogging::testSetDebugCategoriesSelective()
{
    oap::setDebugCategories({"aa", "bt"});

    QVERIFY(lcAA().isDebugEnabled());
    QVERIFY(lcBT().isDebugEnabled());
    QVERIFY(!lcAudio().isDebugEnabled());
    QVERIFY(!lcPlugin().isDebugEnabled());
    QVERIFY(!lcUI().isDebugEnabled());
    QVERIFY(!lcCore().isDebugEnabled());
}

void TestLogging::testSetDebugCategoriesAaEnablesLibrary()
{
    // When "aa" is in the list, oaa.* should also be enabled
    oap::setDebugCategories({"aa"});

    // Check a hypothetical oaa category via filter rules
    // We can verify by checking if the filter would enable it
    QLoggingCategory testOaa("oaa.test", QtInfoMsg);
    QVERIFY(testOaa.isDebugEnabled());
}

void TestLogging::testLibraryDetectionByCategory()
{
    QVERIFY(oap::isLibraryMessage("oaa.messenger", nullptr, QString()));
    QVERIFY(oap::isLibraryMessage("oaa.transport", nullptr, QString()));
}

void TestLogging::testLibraryDetectionByFilePath()
{
    QVERIFY(oap::isLibraryMessage("default", "/path/to/prodigy-oaa-protocol/src/Transport.cpp", QString()));
    QVERIFY(oap::isLibraryMessage("default", "libs/prodigy-oaa-protocol/include/Foo.hpp", QString()));
}

void TestLogging::testLibraryDetectionByBracketTag()
{
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("[TCPTransport] connecting...")));
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("[ControlChannel] opened")));
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("[AASession] RX frame")));
}

void TestLogging::testLibraryDetectionByColonPrefix()
{
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("Messenger: assembled frame")));
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("FrameAssembler: duplicate FIRST")));
    // Should NOT match colon in middle of message
    QVERIFY(!oap::isLibraryMessage("default", nullptr, QStringLiteral("Some prefix Messenger: not at start")));
}

void TestLogging::testLibraryDetectionNewTags()
{
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("[PhoneStatusChannel] call state changed")));
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("[NavChannel] navigation started")));
    QVERIFY(oap::isLibraryMessage("default", nullptr, QStringLiteral("[MediaStatusChannel] playback state")));
}

void TestLogging::testNonLibraryMessage()
{
    QVERIFY(!oap::isLibraryMessage("oap.aa", nullptr, QStringLiteral("Starting AA service")));
    QVERIFY(!oap::isLibraryMessage("default", nullptr, QStringLiteral("Application started")));
    QVERIFY(!oap::isLibraryMessage("default", "src/main.cpp", QStringLiteral("Hello")));
}

void TestLogging::cleanupTestCase()
{
    // Restore quiet defaults
    oap::setVerbose(false);
}

QTEST_GUILESS_MAIN(TestLogging)
#include "test_logging.moc"
