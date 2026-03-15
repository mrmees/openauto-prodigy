// tests/test_widget_contract_qml.cpp -- QML integration test for WidgetInstanceContext injection
#include <QtTest/QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include "ui/WidgetInstanceContext.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetContractQml : public QObject {
    Q_OBJECT
private slots:
    void testContextInjectionThroughLoader();
    void testColSpanPropagation();
    void testIsCurrentPagePropagation();
};

static const char* QML_DELEGATE = R"(
import QtQuick 2.15

Item {
    id: delegate
    width: 200
    height: 200
    property QtObject widgetCtx: null

    onWidgetCtxChanged: {
        if (widgetLoader.item && "widgetContext" in widgetLoader.item)
            widgetLoader.item.widgetContext = widgetCtx
    }

    Loader {
        id: widgetLoader
        objectName: "widgetLoader"
        sourceComponent: Component {
            Item {
                property QtObject widgetContext: null
                property int observedColSpan: widgetContext ? widgetContext.colSpan : -1
                property bool observedIsCurrentPage: widgetContext ? widgetContext.isCurrentPage : false
            }
        }
        onLoaded: {
            if (item && "widgetContext" in item)
                item.widgetContext = delegate.widgetCtx
        }
    }
}
)";

void TestWidgetContractQml::testContextInjectionThroughLoader() {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray(QML_DELEGATE), QUrl());

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> root(component.create());
    QVERIFY(root);

    // Create context
    oap::GridPlacement placement;
    placement.instanceId = "test-qml";
    placement.widgetId = "org.openauto.clock";
    placement.colSpan = 2;
    placement.rowSpan = 3;

    auto* ctx = new oap::WidgetInstanceContext(placement, 100, 100, nullptr, root.data());

    // Set widgetCtx property, which triggers Loader.onLoaded injection
    root->setProperty("widgetCtx", QVariant::fromValue(static_cast<QObject*>(ctx)));

    // Process events to let Loader complete
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject*>() != nullptr, 1000);

    // Get the Loader's loaded item
    auto* loader = root->findChild<QObject*>("widgetLoader");
    QVERIFY(loader);
    QObject* loadedItem = loader->property("item").value<QObject*>();
    QVERIFY(loadedItem);

    // Verify widgetContext was injected
    QObject* injectedCtx = loadedItem->property("widgetContext").value<QObject*>();
    QVERIFY(injectedCtx != nullptr);
    QCOMPARE(injectedCtx, static_cast<QObject*>(ctx));
}

void TestWidgetContractQml::testColSpanPropagation() {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray(QML_DELEGATE), QUrl());

    QScopedPointer<QObject> root(component.create());
    QVERIFY(root);

    oap::GridPlacement placement;
    placement.instanceId = "test-qml-span";
    placement.colSpan = 2;
    placement.rowSpan = 3;

    auto* ctx = new oap::WidgetInstanceContext(placement, 100, 100, nullptr, root.data());
    root->setProperty("widgetCtx", QVariant::fromValue(static_cast<QObject*>(ctx)));

    QCoreApplication::processEvents();

    auto* loader = root->findChild<QObject*>("widgetLoader");
    QVERIFY(loader);
    QObject* loadedItem = loader->property("item").value<QObject*>();
    QVERIFY(loadedItem);

    // Initial value should propagate
    QCOMPARE(loadedItem->property("observedColSpan").toInt(), 2);

    // Live update should propagate through binding
    ctx->setColSpan(4);
    QCOMPARE(loadedItem->property("observedColSpan").toInt(), 4);
}

void TestWidgetContractQml::testIsCurrentPagePropagation() {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray(QML_DELEGATE), QUrl());

    QScopedPointer<QObject> root(component.create());
    QVERIFY(root);

    oap::GridPlacement placement;
    placement.instanceId = "test-qml-page";
    placement.colSpan = 1;
    placement.rowSpan = 1;

    auto* ctx = new oap::WidgetInstanceContext(placement, 100, 100, nullptr, root.data());
    root->setProperty("widgetCtx", QVariant::fromValue(static_cast<QObject*>(ctx)));

    QCoreApplication::processEvents();

    auto* loader = root->findChild<QObject*>("widgetLoader");
    QVERIFY(loader);
    QObject* loadedItem = loader->property("item").value<QObject*>();
    QVERIFY(loadedItem);

    // Default should be false
    QCOMPARE(loadedItem->property("observedIsCurrentPage").toBool(), false);

    // Live update
    ctx->setIsCurrentPage(true);
    QCOMPARE(loadedItem->property("observedIsCurrentPage").toBool(), true);
}

QTEST_MAIN(TestWidgetContractQml)
#include "test_widget_contract_qml.moc"
