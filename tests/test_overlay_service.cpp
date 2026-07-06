#include <QtTest/QtTest>
#include "core/services/OverlayService.hpp"
#include "core/services/ActionRegistry.hpp"

using OS = oap::OverlayService;

static OS::OverlayDescriptor desc(const QString& id, OS::ZBand band) {
    OS::OverlayDescriptor d;
    d.id = id;
    d.qmlComponent = QUrl("qrc:/x/" + id + ".qml");
    d.band = band;
    return d;
}

class TestOverlayService : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndRoles();
    void testDuplicateRejected();
    void testBandZAssignment();
    void testVisibilityAndSignal();
    void testActionsAutoRegistered();
    void testUnregisterRemovesActions();
    void testMoveGeometry();
    void testUnregisterReregisterNoCollision();
    void testReentrantCorrectiveHide();
};

void TestOverlayService::testRegisterAndRoles() {
    oap::ActionRegistry ar; OS svc(&ar);
    QVERIFY(svc.registerOverlay(desc("pairing", OS::ZBand::SystemModal)));
    QCOMPARE(svc.rowCount(), 1);
    auto idx = svc.index(0, 0);
    QCOMPARE(svc.data(idx, OS::OverlayIdRole).toString(), QString("pairing"));
    QCOMPARE(svc.data(idx, OS::VisibleRole).toBool(), false);
    QCOMPARE(svc.data(idx, OS::ZRole).toInt(), 3000);
}

void TestOverlayService::testDuplicateRejected() {
    oap::ActionRegistry ar; OS svc(&ar);
    QVERIFY(svc.registerOverlay(desc("a", OS::ZBand::User)));
    QVERIFY(!svc.registerOverlay(desc("a", OS::ZBand::User)));
    QCOMPARE(svc.rowCount(), 1);
}

void TestOverlayService::testBandZAssignment() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("u1", OS::ZBand::User));       // 2000
    svc.registerOverlay(desc("u2", OS::ZBand::User));       // 2001 (registration order)
    svc.registerOverlay(desc("n1", OS::ZBand::Notifications)); // 1000
    QCOMPARE(svc.data(svc.index(0,0), OS::ZRole).toInt(), 2000);
    QCOMPARE(svc.data(svc.index(1,0), OS::ZRole).toInt(), 2001);
    QCOMPARE(svc.data(svc.index(2,0), OS::ZRole).toInt(), 1000);
}

void TestOverlayService::testVisibilityAndSignal() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));
    QSignalSpy spy(&svc, &OS::overlayVisibilityChanged);
    QSignalSpy dataSpy(&svc, &OS::dataChanged);
    svc.setVisible("a", true);
    QVERIFY(svc.isVisible("a"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(dataSpy.count() >= 1);
    svc.setVisible("a", true);          // no-op — no re-emit
    QCOMPARE(spy.count(), 1);
    svc.toggle("a");
    QVERIFY(!svc.isVisible("a"));
    svc.setVisible("nope", true);       // unknown id: logged, no crash
}

void TestOverlayService::testActionsAutoRegistered() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("pairing", OS::ZBand::SystemModal));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.show"));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.hide"));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.toggle"));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.move"));
    QVERIFY(ar.dispatch("overlay.pairing.show"));
    QVERIFY(svc.isVisible("pairing"));
    QVERIFY(ar.dispatch("overlay.pairing.hide"));
    QVERIFY(!svc.isVisible("pairing"));
    QVERIFY(ar.dispatch("overlay.pairing.toggle"));
    QVERIFY(svc.isVisible("pairing"));
}

void TestOverlayService::testUnregisterRemovesActions() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));
    svc.unregisterOverlay("a");
    QCOMPARE(svc.rowCount(), 0);
    QVERIFY(!ar.registeredActions().contains("overlay.a.show"));
    QVERIFY(!ar.dispatch("overlay.a.show"));
}

void TestOverlayService::testMoveGeometry() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));
    QVariantMap g{{"x", 10}, {"y", 20}, {"width", 300}, {"height", 200}};
    QVERIFY(ar.dispatch("overlay.a.move", g));
    QCOMPARE(svc.data(svc.index(0,0), OS::GeometryRole).toMap().value("x").toInt(), 10);
}

void TestOverlayService::testUnregisterReregisterNoCollision() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("u1", OS::ZBand::User));       // 2000
    svc.registerOverlay(desc("u2", OS::ZBand::User));       // 2001
    QCOMPARE(svc.data(svc.index(0,0), OS::ZRole).toInt(), 2000);
    QCOMPARE(svc.data(svc.index(1,0), OS::ZRole).toInt(), 2001);

    QSignalSpy dataSpy(&svc, &OS::dataChanged);
    svc.unregisterOverlay("u1");
    // u2 renormalizes down to 2000 (band base + its new position among survivors)
    QCOMPARE(svc.rowCount(), 1);
    QCOMPARE(svc.data(svc.index(0,0), OS::ZRole).toInt(), 2000);
    bool sawZRoleChange = false;
    for (const auto& call : dataSpy) {
        const auto roles = call.at(2).value<QVector<int>>();
        if (roles.contains(OS::ZRole)) sawZRoleChange = true;
    }
    QVERIFY(sawZRoleChange);

    QVERIFY(svc.registerOverlay(desc("u3", OS::ZBand::User)));
    QCOMPARE(svc.rowCount(), 2);
    const int u2Z = svc.data(svc.index(0,0), OS::ZRole).toInt();
    const int u3Z = svc.data(svc.index(1,0), OS::ZRole).toInt();
    QCOMPARE(u2Z, 2000);
    QCOMPARE(u3Z, 2001);
    QVERIFY(u2Z != u3Z);
    QCOMPARE(svc.data(svc.index(0,0), OS::OverlayIdRole).toString(), QString("u2"));
    QCOMPARE(svc.data(svc.index(1,0), OS::OverlayIdRole).toString(), QString("u3"));
}

void TestOverlayService::testReentrantCorrectiveHide() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));

    // Spy created before the corrective connect so it records both emissions.
    QSignalSpy spy(&svc, &OS::overlayVisibilityChanged);

    QObject::connect(&svc, &OS::overlayVisibilityChanged, [&](const QString& id, bool visible) {
        if (visible)
            ar.dispatch("overlay.a.hide");
    });

    // No hang/crash: the test completing is itself the assertion that the
    // reentrant corrective dispatch terminates rather than recursing forever.
    QVERIFY(ar.dispatch("overlay.a.show"));

    QVERIFY(!svc.isVisible("a"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(1).toBool(), true);
    QCOMPARE(spy.at(1).at(1).toBool(), false);
}

QTEST_MAIN(TestOverlayService)
#include "test_overlay_service.moc"
