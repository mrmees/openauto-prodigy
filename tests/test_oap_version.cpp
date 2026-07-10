// Verifies the OAP_VERSION compile definition is present and well-formed.
// Named test_oap_version (not test_version) because the prodigy-oaa-protocol
// submodule already owns a CMake target `test_version`; CMake target names are
// global, so the app-side version test must use a distinct name.
// Beta transition: the ALPHA prefix in the regex changes with the scheme
// (see AGENTS.md § Versioning).
#include <QtTest>
#include <QRegularExpression>

class TestVersion : public QObject
{
    Q_OBJECT
private slots:
    void testVersionDefineWellFormed();
};

void TestVersion::testVersionDefineWellFormed()
{
    const QString v = QStringLiteral(OAP_VERSION);
    // Accepts: ALPHA-YY-MM-DD-NN            (tagged build)
    //          ALPHA-YY-MM-DD-NN-<n>-g<hash> (n commits past the tag)
    //          either of the above + -dirty  (uncommitted tree)
    //          ALPHA-untagged-<hash|unknown> (no milestone tag yet)
    static const QRegularExpression re(QStringLiteral(
        "^ALPHA-(untagged-([0-9a-f]+|unknown)"
        "|[0-9]{2}-[0-9]{2}-[0-9]{2}-[0-9]{2,}(-[0-9]+-g[0-9a-f]+)?(-dirty)?)$"));
    QVERIFY2(re.match(v).hasMatch(),
             qPrintable(QStringLiteral("unexpected version: ") + v));
}

QTEST_APPLESS_MAIN(TestVersion)
#include "test_oap_version.moc"
