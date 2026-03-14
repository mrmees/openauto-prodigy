// tests/test_projection_status_provider.cpp
#include <QtTest/QtTest>
#include "core/services/IProjectionStatusProvider.hpp"
#include "core/services/INavigationProvider.hpp"
#include "core/services/IMediaStatusProvider.hpp"
#include "core/services/ICallStateProvider.hpp"
#include "core/services/ProjectionStatusProvider.hpp"
#include "core/plugin/HostContext.hpp"

// Minimal mock that emits the signals ProjectionStatusProvider listens to
class MockOrchestrator : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
public:
    enum ConnectionState { Disconnected = 0, WaitingForDevice, Connecting, Connected, Backgrounded };
    Q_ENUM(ConnectionState)

    int connectionState() const { return state_; }
    QString statusMessage() const { return msg_; }

    void setStateAndMessage(int s, const QString& m) {
        state_ = s;
        msg_ = m;
        emit connectionStateChanged();
        emit statusMessageChanged();
    }

signals:
    void connectionStateChanged();
    void statusMessageChanged();

private:
    int state_ = Disconnected;
    QString msg_;
};

class TestProjectionStatusProvider : public QObject {
    Q_OBJECT
private slots:
    void testInterfaceDefaultState();
    void testProviderReflectsOrchestrator();
    void testProviderSignalsOnChange();
    void testHostContextProviderAccessors();
    void testHostContextProvidersDefaultNull();
};

void TestProjectionStatusProvider::testInterfaceDefaultState() {
    MockOrchestrator orch;
    oap::ProjectionStatusProvider provider(&orch);

    QCOMPARE(provider.projectionState(), oap::IProjectionStatusProvider::Disconnected);
    QVERIFY(provider.statusMessage().isEmpty());
}

void TestProjectionStatusProvider::testProviderReflectsOrchestrator() {
    MockOrchestrator orch;
    oap::ProjectionStatusProvider provider(&orch);

    orch.setStateAndMessage(MockOrchestrator::Connected, "Connected to phone");
    QCOMPARE(provider.projectionState(), oap::IProjectionStatusProvider::Connected);
    QCOMPARE(provider.statusMessage(), "Connected to phone");

    orch.setStateAndMessage(MockOrchestrator::Backgrounded, "Backgrounded");
    QCOMPARE(provider.projectionState(), oap::IProjectionStatusProvider::Backgrounded);
}

void TestProjectionStatusProvider::testProviderSignalsOnChange() {
    MockOrchestrator orch;
    oap::ProjectionStatusProvider provider(&orch);

    QSignalSpy stateSpy(&provider, &oap::IProjectionStatusProvider::projectionStateChanged);
    QSignalSpy msgSpy(&provider, &oap::IProjectionStatusProvider::statusMessageChanged);

    orch.setStateAndMessage(MockOrchestrator::Connected, "Phone connected");
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(msgSpy.count(), 1);
}

void TestProjectionStatusProvider::testHostContextProviderAccessors() {
    oap::HostContext ctx;
    MockOrchestrator orch;
    oap::ProjectionStatusProvider provider(&orch);

    ctx.setProjectionStatusProvider(&provider);
    QCOMPARE(ctx.projectionStatusProvider(), &provider);
}

void TestProjectionStatusProvider::testHostContextProvidersDefaultNull() {
    oap::HostContext ctx;
    QCOMPARE(ctx.projectionStatusProvider(), nullptr);
    QCOMPARE(ctx.navigationProvider(), nullptr);
    QCOMPARE(ctx.mediaStatusProvider(), nullptr);
    QCOMPARE(ctx.callStateProvider(), nullptr);
}

QTEST_GUILESS_MAIN(TestProjectionStatusProvider)
#include "test_projection_status_provider.moc"
