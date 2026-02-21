#include <QTest>
#include "core/services/ActionRegistry.hpp"

class TestActionRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndDispatch()
    {
        oap::ActionRegistry registry;
        bool called = false;
        registry.registerAction("app.quit", [&](const QVariant&) { called = true; });

        bool ok = registry.dispatch("app.quit");
        QVERIFY(ok);
        QVERIFY(called);
    }

    void testDispatchUnknownAction()
    {
        oap::ActionRegistry registry;
        bool ok = registry.dispatch("nonexistent");
        QVERIFY(!ok);
    }

    void testDispatchWithPayload()
    {
        oap::ActionRegistry registry;
        QVariant received;
        registry.registerAction("volume.set", [&](const QVariant& v) { received = v; });

        registry.dispatch("volume.set", 75);
        QCOMPARE(received.toInt(), 75);
    }

    void testUnregister()
    {
        oap::ActionRegistry registry;
        int count = 0;
        registry.registerAction("test", [&](const QVariant&) { ++count; });

        registry.dispatch("test");
        QCOMPARE(count, 1);

        registry.unregisterAction("test");
        bool ok = registry.dispatch("test");
        QVERIFY(!ok);
        QCOMPARE(count, 1);
    }

    void testListActions()
    {
        oap::ActionRegistry registry;
        registry.registerAction("a", [](const QVariant&) {});
        registry.registerAction("b", [](const QVariant&) {});

        auto actions = registry.registeredActions();
        QCOMPARE(actions.size(), 2);
        QVERIFY(actions.contains("a"));
        QVERIFY(actions.contains("b"));
    }

    void testDuplicateRegistrationOverwrites()
    {
        oap::ActionRegistry registry;
        int version = 0;
        registry.registerAction("test", [&](const QVariant&) { version = 1; });
        registry.registerAction("test", [&](const QVariant&) { version = 2; });

        registry.dispatch("test");
        QCOMPARE(version, 2);  // last-write-wins
        QCOMPARE(registry.registeredActions().size(), 1);
    }
};

QTEST_MAIN(TestActionRegistry)
#include "test_action_registry.moc"
