#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Transport/ReplayTransport.hpp>

class TestReplayTransport : public QObject {
    Q_OBJECT
private slots:
    void testFeedData()
    {
        oaa::ReplayTransport transport;
        QSignalSpy dataReceivedSpy(&transport, &oaa::ITransport::dataReceived);

        transport.start();
        transport.feedData("test payload");

        QCOMPARE(dataReceivedSpy.count(), 1);
        QCOMPARE(dataReceivedSpy.at(0).at(0).toByteArray(), QByteArray("test payload"));
    }

    void testWriteCapture()
    {
        oaa::ReplayTransport transport;
        transport.start();

        transport.write("first");
        transport.write("second");

        QCOMPARE(transport.writtenData().size(), 2);
        QCOMPARE(transport.writtenData().at(0), QByteArray("first"));
        QCOMPARE(transport.writtenData().at(1), QByteArray("second"));

        transport.clearWritten();
        QCOMPARE(transport.writtenData().size(), 0);
    }

    void testSimulateConnect()
    {
        oaa::ReplayTransport transport;
        QSignalSpy connectedSpy(&transport, &oaa::ITransport::connected);
        QSignalSpy disconnectedSpy(&transport, &oaa::ITransport::disconnected);

        QVERIFY(!transport.isConnected());

        transport.simulateConnect();
        QVERIFY(transport.isConnected());
        QCOMPARE(connectedSpy.count(), 1);

        transport.simulateDisconnect();
        QVERIFY(!transport.isConnected());
        QCOMPARE(disconnectedSpy.count(), 1);
    }
};

QTEST_MAIN(TestReplayTransport)
#include "test_replay_transport.moc"
