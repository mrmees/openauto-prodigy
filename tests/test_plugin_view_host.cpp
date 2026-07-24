#include <QtTest>

#include <QFile>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "ui/PluginViewHost.hpp"

class DispatchSource : public QObject {
    Q_OBJECT
signals:
    void dispatch();
};

class TestPluginViewHost : public QObject {
    Q_OBJECT
private slots:
    void clearDetachesImmediatelyAndDeletesLater();
    void replacementIsIndependentOfOutgoingView();
    void hostDestructionReclaimsActiveAndPendingViews();

private:
    static QUrl writeItem(QTemporaryDir& directory);
    static QQuickItem* newestChild(QQuickItem& host, QQuickItem* except = nullptr);
    static void deliverDeferredDeletes();
};

QUrl TestPluginViewHost::writeItem(QTemporaryDir& directory)
{
    const QString path = directory.filePath(QStringLiteral("PluginView.qml"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write("import QtQuick\n"
               "Item { objectName: pluginMarker; property string marker: pluginMarker }\n");
    file.close();
    return QUrl::fromLocalFile(path);
}

QQuickItem* TestPluginViewHost::newestChild(QQuickItem& host, QQuickItem* except)
{
    const auto children = host.childItems();
    for (auto it = children.crbegin(); it != children.crend(); ++it) {
        if (*it != except)
            return *it;
    }
    return nullptr;
}

void TestPluginViewHost::deliverDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

void TestPluginViewHost::clearDetachesImmediatelyAndDeletesLater()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl itemUrl = writeItem(directory);
    QVERIFY(itemUrl.isValid());

    QQmlEngine engine;
    QQmlContext context(engine.rootContext());
    context.setContextProperty(QStringLiteral("pluginMarker"), QStringLiteral("first"));
    QQuickItem visualHost;
    visualHost.setWidth(640);
    visualHost.setHeight(360);

    oap::PluginViewHost viewHost(&engine);
    viewHost.setHostItem(&visualHost);
    QVERIFY(viewHost.loadView(itemUrl, &context));

    auto* item = newestChild(visualHost);
    QVERIFY(item);
    QPointer<QQuickItem> guardedItem(item);
    QCOMPARE(item->parentItem(), &visualHost);
    QCOMPARE(item->parent(), &viewHost);
    QCOMPARE(item->width(), 640.0);
    QCOMPARE(item->height(), 360.0);
    QCOMPARE(item->property("marker").toString(), QStringLiteral("first"));

    QSignalSpy cleared(&viewHost, &oap::PluginViewHost::viewCleared);
    DispatchSource source;
    bool laterSlotSawDetached = false;
    bool laterSlotSawTreeAlive = false;
    connect(&source, &DispatchSource::dispatch,
            &viewHost, &oap::PluginViewHost::clearView);
    connect(&source, &DispatchSource::dispatch, this, [&]() {
        laterSlotSawDetached = !viewHost.hasView();
        laterSlotSawTreeAlive = !guardedItem.isNull();
    });
    emit source.dispatch();

    QVERIFY(!viewHost.hasView());
    QCOMPARE(cleared.count(), 1);
    QVERIFY(laterSlotSawDetached);
    QVERIFY(laterSlotSawTreeAlive);
    QVERIFY(guardedItem); // survives the dispatch that requested clear
    QCOMPARE(guardedItem->parentItem(), &visualHost);

    deliverDeferredDeletes();
    QVERIFY(guardedItem.isNull());
    QCOMPARE(visualHost.childItems().size(), 0);
}

void TestPluginViewHost::replacementIsIndependentOfOutgoingView()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl itemUrl = writeItem(directory);

    QQmlEngine engine;
    QQmlContext firstContext(engine.rootContext());
    firstContext.setContextProperty(QStringLiteral("pluginMarker"), QStringLiteral("first"));
    QQmlContext secondContext(engine.rootContext());
    secondContext.setContextProperty(QStringLiteral("pluginMarker"), QStringLiteral("second"));
    QQuickItem visualHost;
    visualHost.setWidth(800);
    visualHost.setHeight(480);

    oap::PluginViewHost viewHost(&engine);
    viewHost.setHostItem(&visualHost);
    QVERIFY(viewHost.loadView(itemUrl, &firstContext));
    QPointer<QQuickItem> outgoing(newestChild(visualHost));
    QVERIFY(outgoing);

    QVERIFY(viewHost.loadView(itemUrl, &secondContext));
    QPointer<QQuickItem> replacement(newestChild(visualHost, outgoing));
    QVERIFY(replacement);
    QVERIFY(viewHost.hasView());
    QVERIFY(outgoing); // old tree is still deferred
    QCOMPARE(replacement->property("marker").toString(), QStringLiteral("second"));

    visualHost.setWidth(1024);
    visualHost.setHeight(600);
    QCOMPARE(replacement->width(), 1024.0);
    QCOMPARE(replacement->height(), 600.0);

    deliverDeferredDeletes();
    QVERIFY(outgoing.isNull());
    QVERIFY(replacement);
    QVERIFY(viewHost.hasView());

    viewHost.clearView();
    deliverDeferredDeletes();
}

void TestPluginViewHost::hostDestructionReclaimsActiveAndPendingViews()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl itemUrl = writeItem(directory);

    QQmlEngine engine;
    QQmlContext context(engine.rootContext());
    context.setContextProperty(QStringLiteral("pluginMarker"), QStringLiteral("owned"));
    QQuickItem visualHost;

    auto* viewHost = new oap::PluginViewHost(&engine);
    viewHost->setHostItem(&visualHost);
    QVERIFY(viewHost->loadView(itemUrl, &context));
    QPointer<QQuickItem> pending(newestChild(visualHost));
    viewHost->clearView();
    QVERIFY(pending);

    QVERIFY(viewHost->loadView(itemUrl, &context));
    QPointer<QQuickItem> active(newestChild(visualHost, pending));
    QVERIFY(active);

    delete viewHost;
    QVERIFY(pending.isNull());
    QVERIFY(active.isNull());
    QCOMPARE(visualHost.childItems().size(), 0);

    // A stale DeferredDelete event for the already-owned pending view is safe.
    deliverDeferredDeletes();
}

QTEST_MAIN(TestPluginViewHost)
#include "test_plugin_view_host.moc"
