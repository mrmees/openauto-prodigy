#include <QtTest>
#include <QFile>
#include <QPointer>
#include <QQmlContext>
#include <QSignalSpy>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTemporaryDir>
#include "ui/PluginModel.hpp"
#include "ui/PluginViewHost.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IHostContext.hpp"

// Mock plugin for testing
class MockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id_;
    QString name_;
    QUrl qmlUrl_;
    QString marker_;
    QPointer<QQmlContext> runtimeContext_;
    QPointer<QQuickItem> viewAtDeactivation_;
    int activationCount_ = 0;
    int deactivationCount_ = 0;
    bool viewGoneBeforeDeactivation_ = false;
    bool contextAliveDuringDeactivation_ = false;

    MockPlugin(const QString& id, const QString& name, QObject* parent = nullptr)
        : QObject(parent), id_(id), name_(name) {}

    QString id() const override { return id_; }
    QString name() const override { return name_; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return qmlUrl_; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }

    void onActivated(QQmlContext* context) override
    {
        runtimeContext_ = context;
        ++activationCount_;
        context->setContextProperty(QStringLiteral("pluginMarker"), marker_);
    }

    void onDeactivated() override
    {
        ++deactivationCount_;
        viewGoneBeforeDeactivation_ = viewAtDeactivation_.isNull();
        contextAliveDuringDeactivation_ = !runtimeContext_.isNull();
    }
};

class ModelDispatchSource : public QObject {
    Q_OBJECT
signals:
    void dispatch();
};

// Minimal mock host context
class MockHostContext : public oap::IHostContext {
public:
    oap::IAudioService* audioService() override { return nullptr; }
    oap::IBluetoothService* bluetoothService() override { return nullptr; }
    oap::IConfigService* configService() override { return nullptr; }
    oap::IThemeService* themeService() override { return nullptr; }
    oap::IDisplayService* displayService() override { return nullptr; }
    oap::IEventBus* eventBus() override { return nullptr; }
    oap::ActionRegistry* actionRegistry() override { return nullptr; }
    oap::INotificationService* notificationService() override { return nullptr; }
    oap::IEqualizerService* equalizerService() override { return nullptr; }
    oap::IProjectionStatusProvider* projectionStatusProvider() override { return nullptr; }
    oap::INavigationProvider* navigationProvider() override { return nullptr; }
    oap::IMediaStatusProvider* mediaStatusProvider() override { return nullptr; }
    oap::ICallStateProvider* callStateProvider() override { return nullptr; }
    oap::OverlayService* overlayService() override { return nullptr; }
    void log(oap::LogLevel, const QString&) override {}
};

class TestPluginModel : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(qmlDirectory_.isValid());
        qmlUrl_ = writePluginView();
        QVERIFY(qmlUrl_.isValid());
    }

    void testRowCountMatchesPlugins()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);
        QCOMPARE(model.rowCount(), 1);
    }

    void testSetActivePluginValid()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);
        QSignalSpy spy(&model, &oap::PluginModel::activePluginChanged);

        model.setActivePlugin("test.a");
        QCOMPARE(model.activePluginId(), QString("test.a"));
        QCOMPARE(spy.count(), 1);
    }

    void testSetActivePluginInvalid()
    {
        oap::PluginManager manager;
        QQmlEngine engine;
        oap::PluginModel model(&manager, &engine);

        model.setActivePlugin("nonexistent");
        QCOMPARE(model.activePluginId(), QString());  // unchanged
    }

    void testSetActivePluginEmpty()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);

        model.setActivePlugin("test.a");
        model.setActivePlugin(QString());  // go home
        QCOMPARE(model.activePluginId(), QString());
    }

    void testSettingsQmlRole()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);
        QModelIndex idx = model.index(0, 0);

        // MockPlugin inherits default settingsComponent(), which is empty.
        QVariant val = model.data(idx, oap::PluginModel::SettingsQmlRole);
        QVERIFY(val.isValid());
        QCOMPARE(val.toUrl(), QUrl());
    }

    void testHomeRetiresViewBeforeContextAfterDispatch()
    {
        auto* plugin = new MockPlugin("test.a", "A", this);
        plugin->qmlUrl_ = qmlUrl_;
        plugin->marker_ = QStringLiteral("alpha");
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;
        QQuickItem visualHost;

        manager.registerStaticPlugin(plugin);
        manager.initializeAll(&ctx);
        oap::PluginModel model(&manager, &engine);
        model.viewHost()->setHostItem(&visualHost);
        model.setActivePlugin(plugin->id());

        QPointer<QQuickItem> outgoing = newestChild(visualHost);
        QVERIFY(outgoing);
        plugin->viewAtDeactivation_ = outgoing;
        QCOMPARE(outgoing->property("marker").toString(), QStringLiteral("alpha"));

        ModelDispatchSource source;
        bool laterSawLogicalHome = false;
        bool laterReadViewProperty = false;
        bool laterReadContextProperty = false;
        bool laterSawNotDeactivated = false;
        connect(&source, &ModelDispatchSource::dispatch, &model, [&]() {
            model.setActivePlugin(QString());
        });
        connect(&source, &ModelDispatchSource::dispatch, this, [&]() {
            laterSawLogicalHome = model.activePluginId().isEmpty()
                                  && !model.viewHost()->hasView();
            laterReadViewProperty = outgoing
                                    && outgoing->property("marker").toString()
                                           == QStringLiteral("alpha");
            laterReadContextProperty = plugin->runtimeContext_
                                       && plugin->runtimeContext_->contextProperty(
                                              QStringLiteral("pluginMarker")).toString()
                                              == QStringLiteral("alpha");
            laterSawNotDeactivated = plugin->deactivationCount_ == 0;
        });

        emit source.dispatch();

        QVERIFY(laterSawLogicalHome);
        QVERIFY(laterReadViewProperty);
        QVERIFY(laterReadContextProperty);
        QVERIFY(laterSawNotDeactivated);
        QVERIFY(outgoing);
        QVERIFY(plugin->runtimeContext_);

        QTRY_VERIFY(outgoing.isNull());
        QTRY_COMPARE(plugin->deactivationCount_, 1);
        QVERIFY(plugin->viewGoneBeforeDeactivation_);
        QVERIFY(plugin->contextAliveDuringDeactivation_);
        QTRY_VERIFY(plugin->runtimeContext_.isNull());
    }

    void testReplacementAndRepeatedHomeRetireIndependently()
    {
        auto* first = new MockPlugin("test.a", "A", this);
        auto* second = new MockPlugin("test.b", "B", this);
        first->qmlUrl_ = qmlUrl_;
        first->marker_ = QStringLiteral("alpha");
        second->qmlUrl_ = qmlUrl_;
        second->marker_ = QStringLiteral("beta");
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;
        QQuickItem visualHost;

        manager.registerStaticPlugin(first);
        manager.registerStaticPlugin(second);
        manager.initializeAll(&ctx);
        oap::PluginModel model(&manager, &engine);
        model.viewHost()->setHostItem(&visualHost);

        model.setActivePlugin(first->id());
        QPointer<QQuickItem> firstView = newestChild(visualHost);
        first->viewAtDeactivation_ = firstView;

        model.setActivePlugin(second->id());
        QPointer<QQuickItem> secondView = newestChild(visualHost, firstView);
        second->viewAtDeactivation_ = secondView;
        QCOMPARE(model.activePluginId(), second->id());
        QVERIFY(firstView);
        QVERIFY(first->runtimeContext_);
        QVERIFY(secondView);
        QVERIFY(second->runtimeContext_);
        QCOMPARE(first->deactivationCount_, 0);

        // Queue a second distinct retirement before the first event runs.
        model.setActivePlugin(QString());
        QVERIFY(!model.viewHost()->hasView());
        QVERIFY(secondView);
        QCOMPARE(second->deactivationCount_, 0);

        QTRY_VERIFY(firstView.isNull());
        QTRY_VERIFY(secondView.isNull());
        QTRY_COMPARE(first->deactivationCount_, 1);
        QTRY_COMPARE(second->deactivationCount_, 1);
        QVERIFY(first->viewGoneBeforeDeactivation_);
        QVERIFY(second->viewGoneBeforeDeactivation_);
        QTRY_VERIFY(first->runtimeContext_.isNull());
        QTRY_VERIFY(second->runtimeContext_.isNull());
    }

    void testLoadFailureRetiresContextWithoutPendingView()
    {
        auto* plugin = new MockPlugin("test.bad", "Bad", this);
        plugin->qmlUrl_ = QUrl::fromLocalFile(
            qmlDirectory_.filePath(QStringLiteral("missing.qml")));
        plugin->marker_ = QStringLiteral("bad");
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;
        QQuickItem visualHost;

        manager.registerStaticPlugin(plugin);
        manager.initializeAll(&ctx);
        oap::PluginModel model(&manager, &engine);
        model.viewHost()->setHostItem(&visualHost);
        model.setActivePlugin(plugin->id());

        QVERIFY(model.activePluginId().isEmpty());
        QVERIFY(!model.viewHost()->hasView());
        QCOMPARE(plugin->activationCount_, 1);
        QCOMPARE(plugin->deactivationCount_, 1);
        QVERIFY(plugin->contextAliveDuringDeactivation_);
        QVERIFY(plugin->runtimeContext_.isNull());
    }

    void testModelTeardownDrainsViewBeforeContext()
    {
        auto* first = new MockPlugin("test.a", "A", this);
        auto* second = new MockPlugin("test.b", "B", this);
        first->qmlUrl_ = qmlUrl_;
        first->marker_ = QStringLiteral("first");
        second->qmlUrl_ = qmlUrl_;
        second->marker_ = QStringLiteral("second");
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;
        QQuickItem visualHost;

        manager.registerStaticPlugin(first);
        manager.registerStaticPlugin(second);
        manager.initializeAll(&ctx);
        auto* model = new oap::PluginModel(&manager, &engine);
        model->viewHost()->setHostItem(&visualHost);
        model->setActivePlugin(first->id());
        QPointer<QQuickItem> pendingView = newestChild(visualHost);
        first->viewAtDeactivation_ = pendingView;
        model->setActivePlugin(second->id());
        QPointer<QQuickItem> activeView = newestChild(visualHost, pendingView);
        second->viewAtDeactivation_ = activeView;
        QVERIFY(pendingView);
        QVERIFY(activeView);
        QVERIFY(first->runtimeContext_);
        QVERIFY(second->runtimeContext_);

        delete model;

        QVERIFY(pendingView.isNull());
        QVERIFY(activeView.isNull());
        QCOMPARE(first->deactivationCount_, 1);
        QCOMPARE(second->deactivationCount_, 1);
        QVERIFY(first->viewGoneBeforeDeactivation_);
        QVERIFY(second->viewGoneBeforeDeactivation_);
        QVERIFY(first->contextAliveDuringDeactivation_);
        QVERIFY(second->contextAliveDuringDeactivation_);
        QVERIFY(first->runtimeContext_.isNull());
        QVERIFY(second->runtimeContext_.isNull());
    }

private:
    QUrl writePluginView()
    {
        const QString path = qmlDirectory_.filePath(QStringLiteral("ModelPluginView.qml"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return {};
        file.write("import QtQuick\n"
                   "Item { property string marker: pluginMarker }\n");
        file.close();
        return QUrl::fromLocalFile(path);
    }

    static QQuickItem* newestChild(QQuickItem& host, QQuickItem* except = nullptr)
    {
        const auto children = host.childItems();
        for (auto it = children.crbegin(); it != children.crend(); ++it) {
            if (*it != except)
                return *it;
        }
        return nullptr;
    }

    QTemporaryDir qmlDirectory_;
    QUrl qmlUrl_;
};

QTEST_MAIN(TestPluginModel)
#include "test_plugin_model.moc"
