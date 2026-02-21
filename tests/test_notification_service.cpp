#include <QTest>
#include "core/services/NotificationService.hpp"
#include "ui/NotificationModel.hpp"

class TestNotificationService : public QObject {
    Q_OBJECT
private slots:
    void testPostNotification()
    {
        oap::NotificationService svc;
        oap::NotificationModel model(&svc);

        QCOMPARE(model.rowCount(), 0);

        svc.post({
            {"kind", "toast"},
            {"message", "Hello"},
            {"sourcePluginId", "test"},
            {"priority", 50},
            {"ttlMs", 5000}
        });

        QCOMPARE(model.rowCount(), 1);
    }

    void testDismiss()
    {
        oap::NotificationService svc;
        oap::NotificationModel model(&svc);

        svc.post({{"kind", "toast"}, {"message", "Test"}});
        QCOMPARE(model.rowCount(), 1);

        auto id = model.data(model.index(0, 0), oap::NotificationModel::NotificationIdRole).toString();
        svc.dismiss(id);
        QCOMPARE(model.rowCount(), 0);
    }

    void testTtlExpiry()
    {
        oap::NotificationService svc;
        oap::NotificationModel model(&svc);

        svc.post({{"kind", "toast"}, {"message", "Ephemeral"}, {"ttlMs", 50}});
        QCOMPARE(model.rowCount(), 1);

        QTest::qWait(100);
        QCOMPARE(model.rowCount(), 0);  // expired
    }
};

QTEST_MAIN(TestNotificationService)
#include "test_notification_service.moc"
