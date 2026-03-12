// tests/test_widget_plugin_integration.cpp
#include <QtTest/QtTest>
#include "core/plugin/IPlugin.hpp"
#include "core/widget/WidgetTypes.hpp"

// Mock plugin that doesn't override widgetDescriptors
class PlainMockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.plain"; }
    QString name() const override { return "Plain"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

// Mock plugin that provides widgets
class WidgetMockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.widgeted"; }
    QString name() const override { return "Widgeted"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }

    QList<oap::WidgetDescriptor> widgetDescriptors() const override {
        oap::WidgetDescriptor desc;
        desc.id = "org.test.widgeted.status";
        desc.displayName = "Status";
        desc.minCols = 1; desc.minRows = 1;
        desc.maxCols = 3; desc.maxRows = 2;
        desc.defaultCols = 2; desc.defaultRows = 1;
        desc.pluginId = id();
        return {desc};
    }
};

class TestWidgetPluginIntegration : public QObject {
    Q_OBJECT
private slots:
    void testDefaultReturnsEmpty();
    void testPluginProvidesWidgets();
};

void TestWidgetPluginIntegration::testDefaultReturnsEmpty() {
    PlainMockPlugin plugin;
    QCOMPARE(plugin.widgetDescriptors().size(), 0);
}

void TestWidgetPluginIntegration::testPluginProvidesWidgets() {
    WidgetMockPlugin plugin;
    auto widgets = plugin.widgetDescriptors();
    QCOMPARE(widgets.size(), 1);
    QCOMPARE(widgets[0].id, "org.test.widgeted.status");
    QCOMPARE(widgets[0].pluginId, "org.test.widgeted");
}

QTEST_GUILESS_MAIN(TestWidgetPluginIntegration)
#include "test_widget_plugin_integration.moc"
