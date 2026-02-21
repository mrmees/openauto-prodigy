#include <QTest>
#include <QSignalSpy>
#include "core/services/EventBus.hpp"

class TestEventBus : public QObject {
    Q_OBJECT
private slots:
    void testSubscribeAndPublish()
    {
        oap::EventBus bus;
        QVariant received;
        int subId = bus.subscribe("test/topic", [&](const QVariant& v) { received = v; });

        bus.publish("test/topic", 42);
        // EventBus delivers via QueuedConnection — need event loop
        QCoreApplication::processEvents();

        QCOMPARE(received.toInt(), 42);
        QVERIFY(subId > 0);
    }

    void testUnsubscribe()
    {
        oap::EventBus bus;
        int count = 0;
        int subId = bus.subscribe("test/topic", [&](const QVariant&) { ++count; });

        bus.publish("test/topic");
        QCoreApplication::processEvents();
        QCOMPARE(count, 1);

        bus.unsubscribe(subId);
        bus.publish("test/topic");
        QCoreApplication::processEvents();
        QCOMPARE(count, 1);  // no change — unsubscribed
    }

    void testMultipleSubscribers()
    {
        oap::EventBus bus;
        int countA = 0, countB = 0;
        bus.subscribe("test/topic", [&](const QVariant&) { ++countA; });
        bus.subscribe("test/topic", [&](const QVariant&) { ++countB; });

        bus.publish("test/topic");
        QCoreApplication::processEvents();

        QCOMPARE(countA, 1);
        QCOMPARE(countB, 1);
    }

    void testTopicIsolation()
    {
        oap::EventBus bus;
        int count = 0;
        bus.subscribe("topic/a", [&](const QVariant&) { ++count; });

        bus.publish("topic/b");
        QCoreApplication::processEvents();
        QCOMPARE(count, 0);  // different topic — not delivered
    }
};

QTEST_MAIN(TestEventBus)
#include "test_event_bus.moc"
