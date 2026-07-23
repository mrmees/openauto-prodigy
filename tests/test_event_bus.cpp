#include <QTest>
#include <QSignalSpy>
#include <QSemaphore>
#include <QThread>
#include <atomic>
#include <thread>
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
        // EventBus delivers via QueuedConnection — QTRY spins event loop until delivered
        QTRY_COMPARE(received.toInt(), 42);
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

    void testUnsubscribeCancelsQueuedDelivery()
    {
        oap::EventBus bus;
        int count = 0;
        const int subId = bus.subscribe("test/topic", [&](const QVariant&) { ++count; });

        bus.publish("test/topic");
        bus.unsubscribe(subId);
        QCoreApplication::processEvents();

        QCOMPARE(count, 0);
    }

    void testSelfUnsubscribeDoesNotDeadlock()
    {
        oap::EventBus bus;
        int count = 0;
        int subId = 0;
        subId = bus.subscribe("test/topic", [&](const QVariant&) {
            ++count;
            bus.unsubscribe(subId);
        });

        bus.publish("test/topic");
        bus.publish("test/topic");
        QCoreApplication::processEvents();

        QCOMPARE(count, 1);
    }

    void testOffThreadUnsubscribeWaitsForExecutingCallback()
    {
        QThread ownerThread;
        auto* bus = new oap::EventBus;
        bus->moveToThread(&ownerThread);
        ownerThread.start();

        QSemaphore entered;
        QSemaphore release;
        std::atomic_bool ranOnOwnerThread = false;
        const int subId = bus->subscribe("test/topic", [&](const QVariant&) {
            ranOnOwnerThread.store(QThread::currentThread() == &ownerThread);
            entered.release();
            release.acquire();
        });
        bus->publish("test/topic");
        const bool callbackEntered = entered.tryAcquire(1, 1000);
        if (!callbackEntered) {
            // Keep failure cleanup safe even if delivery is merely late.
            release.release();
            QMetaObject::invokeMethod(bus, [bus] { delete bus; },
                                      Qt::BlockingQueuedConnection);
            ownerThread.quit();
            ownerThread.wait();
            QVERIFY(callbackEntered);
            return;
        }

        std::atomic_bool unsubscribeReturned = false;
        QSemaphore unsubscribeStarted;
        std::thread unsubscriber([&] {
            unsubscribeStarted.release();
            bus->unsubscribe(subId);
            unsubscribeReturned.store(true);
        });
        QVERIFY(unsubscribeStarted.tryAcquire(1, 1000));
        QTest::qWait(20);
        const bool returnedEarly = unsubscribeReturned.load();

        release.release();
        unsubscriber.join();
        QVERIFY(!returnedEarly);
        QVERIFY(unsubscribeReturned.load());
        QVERIFY(ranOnOwnerThread.load());

        QMetaObject::invokeMethod(bus, [bus] { delete bus; },
                                  Qt::BlockingQueuedConnection);
        ownerThread.quit();
        QVERIFY(ownerThread.wait(1000));
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
