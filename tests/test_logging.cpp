#include <QtTest>
#include "core/Logging.hpp"

#include <cstdio>
#include <unistd.h>

namespace {

class StderrCapture
{
public:
    StderrCapture()
        : file_(tmpfile())
    {
        fflush(stderr);
        savedFd_ = dup(fileno(stderr));
        if (file_ && savedFd_ >= 0)
            dup2(fileno(file_), fileno(stderr));
    }

    ~StderrCapture()
    {
        fflush(stderr);
        if (savedFd_ >= 0) {
            dup2(savedFd_, fileno(stderr));
            close(savedFd_);
        }
        if (file_)
            fclose(file_);
    }

    QByteArray output()
    {
        QByteArray result;
        if (!file_)
            return result;

        fflush(stderr);
        rewind(file_);
        char buffer[256];
        while (const size_t read = fread(buffer, 1, sizeof(buffer), file_))
            result.append(buffer, static_cast<qsizetype>(read));
        return result;
    }

private:
    int savedFd_{-1};
    FILE* file_{nullptr};
};

QByteArray emitLibraryMessage(QLoggingCategory& category, QtMsgType type,
                              const QString& marker)
{
    StderrCapture capture;
    if (type == QtDebugMsg)
        qCDebug(category).noquote() << marker;
    else
        qCInfo(category).noquote() << marker;
    return capture.output();
}

} // namespace

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
    void testLibraryOutputHandlerPolicy();
    void testInvalidDebugCategoriesAreIgnored();
    void testApplyLoggingPolicyRestoresSelectiveCategories();

    // Library message detection
    void testLibraryDetectionByCategory();
    void testLibraryDetectionByFilePath();
    void testLibraryDetectionByBracketTag();
    void testLibraryDetectionByColonPrefix();
    void testLibraryDetectionNewTags();
    void testNonLibraryMessage();

    void cleanup();
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

void TestLogging::testLibraryOutputHandlerPolicy()
{
    QLoggingCategory libraryCategory("oaa.logging-test", QtInfoMsg);
    const QString suppressedMarker = QStringLiteral("library-output-suppressed-without-aa");
    const QString selectiveDebugMarker = QStringLiteral("library-debug-output-with-aa");
    const QString selectiveInfoMarker = QStringLiteral("library-info-output-with-aa");
    const QString verboseMarker = QStringLiteral("library-output-with-global-verbose");

    oap::setDebugCategories({"bt"});
    QVERIFY(!oap::isVerbose());
    QVERIFY(!libraryCategory.isDebugEnabled());
    QVERIFY(!emitLibraryMessage(libraryCategory, QtInfoMsg, suppressedMarker)
                 .contains(suppressedMarker.toUtf8()));

    oap::setDebugCategories({"aa"});
    QVERIFY(!oap::isVerbose());
    QVERIFY(libraryCategory.isDebugEnabled());
    QVERIFY(!lcBT().isDebugEnabled());
    QVERIFY(emitLibraryMessage(libraryCategory, QtDebugMsg, selectiveDebugMarker)
                 .contains(selectiveDebugMarker.toUtf8()));
    QVERIFY(emitLibraryMessage(libraryCategory, QtInfoMsg, selectiveInfoMarker)
                 .contains(selectiveInfoMarker.toUtf8()));

    oap::setVerbose(true);
    QVERIFY(oap::isVerbose());
    QVERIFY(emitLibraryMessage(libraryCategory, QtInfoMsg, verboseMarker)
                 .contains(verboseMarker.toUtf8()));
}

void TestLogging::testInvalidDebugCategoriesAreIgnored()
{
    oap::setDebugCategories({"aa", "oap.core\n*.debug=true", "oap.core"});

    QVERIFY(lcAA().isDebugEnabled());
    QVERIFY(!lcCore().isDebugEnabled());
    QVERIFY(!lcBT().isDebugEnabled());
}

void TestLogging::testApplyLoggingPolicyRestoresSelectiveCategories()
{
    const QStringList categories{"core", "eq"};
    oap::applyLoggingPolicy(true, categories);
    QVERIFY(oap::isVerbose());
    QVERIFY(lcAA().isDebugEnabled());

    // This mirrors the settings callback when logging.verbose changes to false.
    oap::applyLoggingPolicy(false, categories);
    QVERIFY(!oap::isVerbose());
    QVERIFY(lcCore().isDebugEnabled());
    QVERIFY(lcEq().isDebugEnabled());
    QVERIFY(!lcAA().isDebugEnabled());
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

void TestLogging::cleanup()
{
    // Every test leaves the installed handler with quiet defaults.
    oap::setVerbose(false);
}

void TestLogging::cleanupTestCase()
{
    // Restore quiet defaults
    oap::setVerbose(false);
}

QTEST_GUILESS_MAIN(TestLogging)
#include "test_logging.moc"
