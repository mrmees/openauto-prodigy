#include <QtTest>
#include "ui/PairedDevicesModel.hpp"

class TestPairedDevicesModel : public QObject {
    Q_OBJECT
private slots:
    void testEmptyModel();
    void testSetDevices();
    void testDataAccess();
    void testUpdateConnectionState();
    void testRoleNames();
};

void TestPairedDevicesModel::testEmptyModel()
{
    oap::PairedDevicesModel model;
    QCOMPARE(model.rowCount(), 0);
}

void TestPairedDevicesModel::testSetDevices()
{
    oap::PairedDevicesModel model;
    QList<oap::PairedDeviceInfo> devices = {
        {"AA:BB:CC:DD:EE:FF", "Phone 1", false},
        {"11:22:33:44:55:66", "Phone 2", true},
    };
    model.setDevices(devices);
    QCOMPARE(model.rowCount(), 2);
}

void TestPairedDevicesModel::testDataAccess()
{
    oap::PairedDevicesModel model;
    QList<oap::PairedDeviceInfo> devices = {
        {"AA:BB:CC:DD:EE:FF", "Phone 1", false},
        {"11:22:33:44:55:66", "Phone 2", true},
    };
    model.setDevices(devices);

    QModelIndex idx0 = model.index(0, 0);
    QCOMPARE(model.data(idx0, oap::PairedDevicesModel::AddressRole).toString(), QString("AA:BB:CC:DD:EE:FF"));
    QCOMPARE(model.data(idx0, oap::PairedDevicesModel::NameRole).toString(), QString("Phone 1"));
    QCOMPARE(model.data(idx0, oap::PairedDevicesModel::ConnectedRole).toBool(), false);

    QModelIndex idx1 = model.index(1, 0);
    QCOMPARE(model.data(idx1, oap::PairedDevicesModel::ConnectedRole).toBool(), true);
}

void TestPairedDevicesModel::testUpdateConnectionState()
{
    oap::PairedDevicesModel model;
    QList<oap::PairedDeviceInfo> devices = {
        {"AA:BB:CC:DD:EE:FF", "Phone 1", false},
    };
    model.setDevices(devices);

    QCOMPARE(model.data(model.index(0, 0), oap::PairedDevicesModel::ConnectedRole).toBool(), false);

    model.updateConnectionState("AA:BB:CC:DD:EE:FF", true);
    QCOMPARE(model.data(model.index(0, 0), oap::PairedDevicesModel::ConnectedRole).toBool(), true);

    // Non-existent address should not crash
    model.updateConnectionState("00:00:00:00:00:00", true);
    QCOMPARE(model.rowCount(), 1);  // unchanged
}

void TestPairedDevicesModel::testRoleNames()
{
    oap::PairedDevicesModel model;
    auto roles = model.roleNames();
    QCOMPARE(roles[oap::PairedDevicesModel::AddressRole], QByteArray("address"));
    QCOMPARE(roles[oap::PairedDevicesModel::NameRole], QByteArray("name"));
    QCOMPARE(roles[oap::PairedDevicesModel::ConnectedRole], QByteArray("connected"));
}

QTEST_MAIN(TestPairedDevicesModel)
#include "test_paired_devices_model.moc"
