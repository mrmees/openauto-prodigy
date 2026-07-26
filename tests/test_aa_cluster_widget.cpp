#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>
#include <QVideoSink>

#include "core/aa/ProjectedDisplayConfig.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "plugins/android_auto/AAClusterWidgetRegistration.hpp"

class FakeClusterDisplay : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool rendering READ rendering CONSTANT)
    Q_PROPERTY(QString statusText READ statusText CONSTANT)
    Q_PROPERTY(int viewportEncodedWidth READ viewportEncodedWidth
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportEncodedHeight READ viewportEncodedHeight
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentX READ viewportContentX
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentY READ viewportContentY
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentWidth READ viewportContentWidth
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(int viewportContentHeight READ viewportContentHeight
                   NOTIFY viewportGeometryChanged)
    Q_PROPERTY(bool videoSinkAvailable READ videoSinkAvailable
                   NOTIFY videoSinkAvailabilityChanged)

public:
    bool rendering() const { return false; }
    QString statusText() const { return QStringLiteral("Waiting"); }
    int viewportEncodedWidth() const { return geometry_.encodedWidth; }
    int viewportEncodedHeight() const { return geometry_.encodedHeight; }
    int viewportContentX() const { return geometry_.contentX(); }
    int viewportContentY() const { return geometry_.contentY(); }
    int viewportContentWidth() const { return geometry_.contentWidth; }
    int viewportContentHeight() const { return geometry_.contentHeight; }
    void setGeometry(const oap::aa::ProjectedViewportGeometry& geometry)
    {
        geometry_ = geometry;
        emit viewportGeometryChanged();
    }
    bool videoSinkAvailable() const { return sink_.isNull(); }
    Q_INVOKABLE bool attachVideoSink(QVideoSink* sink)
    {
        ++attachAttempts_;
        if (!sink || (sink_ && sink_ != sink))
            return false;
        sink_ = sink;
        emit videoSinkAvailabilityChanged();
        return true;
    }
    Q_INVOKABLE void detachVideoSink(QVideoSink* sink)
    {
        if (sink_ == sink) {
            sink_.clear();
            emit videoSinkAvailabilityChanged();
        }
    }
    QVideoSink* sink() const { return sink_.data(); }
    int attachAttempts() const { return attachAttempts_; }

signals:
    void videoSinkAvailabilityChanged();
    void viewportGeometryChanged();

private:
    oap::aa::ProjectedViewportGeometry geometry_ =
        oap::aa::kClusterViewportGeometry;
    QPointer<QVideoSink> sink_;
    int attachAttempts_ = 0;
};

class FakeWidgetContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isCurrentPage READ isCurrentPage WRITE setIsCurrentPage
                   NOTIFY isCurrentPageChanged)

public:
    bool isCurrentPage() const { return current_; }
    void setIsCurrentPage(bool current)
    {
        if (current_ == current)
            return;
        current_ = current;
        emit isCurrentPageChanged();
    }

signals:
    void isCurrentPageChanged();

private:
    bool current_ = false;
};

class TestAAClusterWidget : public QObject {
    Q_OBJECT

private:
    static QString qmlSource()
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR
                                  "/qml/widgets/AAClusterWidget.qml"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void disabledConfigRegistersNothing()
    {
        oap::WidgetRegistry registry;
        QVERIFY(!oap::plugins::registerAAClusterWidget(
            registry, {false, {}}, true));
        QVERIFY(!registry.descriptor(QStringLiteral("org.openauto.aa-cluster"))
                     .has_value());
        QVERIFY(registry.widgetsFittingSpace(2, 2).isEmpty());
    }

    void enabledConfigRegistersFixedSquare()
    {
        oap::WidgetRegistry registry;
        QVERIFY(oap::plugins::registerAAClusterWidget(
            registry, {true, {}}, true));

        const auto descriptor =
            registry.descriptor(QStringLiteral("org.openauto.aa-cluster"));
        QVERIFY(descriptor.has_value());
        QCOMPARE(descriptor->minCols, 3);
        QCOMPARE(descriptor->maxCols, 3);
        QCOMPARE(descriptor->minRows, 3);
        QCOMPARE(descriptor->maxRows, 3);
        QCOMPARE(descriptor->defaultCols, 3);
        QCOMPARE(descriptor->defaultRows, 3);
        QCOMPARE(descriptor->category, QStringLiteral("navigation"));
        QCOMPARE(descriptor->qmlComponent,
                 QUrl(QStringLiteral(
                     "qrc:/OpenAutoProdigy/AAClusterWidget.qml")));
        QCOMPARE(registry.allDescriptors().size(), 1);
        QVERIFY(registry.widgetsFittingSpace(2, 2).isEmpty());
        const auto pickerEntries = registry.widgetsFittingSpace(3, 3);
        QCOMPARE(pickerEntries.size(), 1);
        QCOMPARE(pickerEntries.first().id,
                 QStringLiteral("org.openauto.aa-cluster"));
        QVERIFY(registry.widgetsFittingSpace(1, 2).isEmpty());
    }

    void unavailableDisplayRegistersNothing()
    {
        oap::WidgetRegistry registry;
        QVERIFY(!oap::plugins::registerAAClusterWidget(
            registry, {true, {}}, false));
        QVERIFY(!registry.descriptor(QStringLiteral("org.openauto.aa-cluster"))
                     .has_value());
    }

    void qmlOwnsOneNonInteractivePreserveAspectSink()
    {
        const QString source = qmlSource();
        QVERIFY2(!source.isEmpty(), "Failed to read AAClusterWidget.qml");

        QVERIFY(source.contains(QStringLiteral("import QtMultimedia")));
        QCOMPARE(source.count(QStringLiteral("VideoOutput {")), 1);
        QVERIFY(source.contains(
            QStringLiteral("fillMode: VideoOutput.PreserveAspectFit")));
        QVERIFY(source.contains(QStringLiteral("AAClusterDisplay.rendering")));
        QVERIFY(source.contains(QStringLiteral(
            "attachVideoSink(videoOutput.videoSink)")));
        QVERIFY(source.contains(QStringLiteral(
            "detachVideoSink(videoOutput.videoSink)")));
        QVERIFY(source.contains(QStringLiteral("widgetContext.isCurrentPage")));
        QVERIFY(source.contains(QStringLiteral("onIsCurrentPageChanged")));
        QVERIFY(source.contains(
            QStringLiteral("onVideoSinkAvailabilityChanged")));
        QVERIFY(source.contains(QStringLiteral("if (!isCurrentPage)")));
        QVERIFY(source.contains(QStringLiteral(
            "root.sinkClaimAttempts < root.maxSinkClaimAttempts")));
        QVERIFY(source.contains(QStringLiteral(
            "onTriggered: root.syncSinkClaim()")));
        QVERIFY(source.contains(QStringLiteral("already in use"),
                                Qt::CaseInsensitive));
        QVERIFY(source.contains(
            QStringLiteral("objectName: \"clusterCropViewport\"")));
        QVERIFY(source.contains(
            QStringLiteral("objectName: \"clusterVideoOutput\"")));
        for (const QString& property : {
                 QStringLiteral("viewportEncodedWidth"),
                 QStringLiteral("viewportEncodedHeight"),
                 QStringLiteral("viewportContentX"),
                 QStringLiteral("viewportContentY"),
                 QStringLiteral("viewportContentWidth"),
                 QStringLiteral("viewportContentHeight")}) {
            QVERIFY2(source.contains(
                         QStringLiteral("AAClusterDisplay.%1").arg(property)),
                     qPrintable(QStringLiteral("Missing geometry property: %1")
                                    .arg(property)));
        }

        const QStringList forbidden = {
            QStringLiteral("MouseArea"),
            QStringLiteral("TapHandler"),
            QStringLiteral("ShaderEffect"),
            QStringLiteral("sourceRect"),
            QStringLiteral("transform:"),
            QStringLiteral("AAClusterDisplay.Rendering"),
        };
        for (const QString& token : forbidden) {
            QVERIFY2(!source.contains(token),
                     qPrintable(QStringLiteral("Forbidden QML token: %1")
                                    .arg(token)));
        }
    }

    void onlyCurrentDashboardCopyOwnsSinkAtRuntime()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto writeFile = [&dir](const QString& name,
                                      const QByteArray& contents) {
            QSaveFile file(dir.filePath(name));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                return false;
            if (file.write(contents) != contents.size())
                return false;
            return file.commit();
        };
        QVERIFY(writeFile(QStringLiteral("AAClusterWidget.qml"),
                          qmlSource().toUtf8()));
        QVERIFY(writeFile(
            QStringLiteral("MaterialIcon.qml"),
            QByteArrayLiteral(
                "import QtQuick\nItem { property string icon; property real "
                "size; property color color; implicitWidth: size; "
                "implicitHeight: size }\n")));
        QVERIFY(writeFile(QStringLiteral("NormalText.qml"),
                          QByteArrayLiteral("import QtQuick\nText {}\n")));

        FakeClusterDisplay display;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(
            QStringLiteral("AAClusterDisplay"), &display);
        engine.rootContext()->setContextProperty(
            QStringLiteral("UiMetrics"),
            QVariantMap{{QStringLiteral("spacing"), 8},
                        {QStringLiteral("iconSize"), 24},
                        {QStringLiteral("fontBody"), 16}});
        engine.rootContext()->setContextProperty(
            QStringLiteral("ThemeService"),
            QVariantMap{{QStringLiteral("onSurfaceVariant"),
                         QStringLiteral("#ffffff")}});

        QQmlComponent component(
            &engine, QUrl::fromLocalFile(
                         dir.filePath(QStringLiteral("AAClusterWidget.qml"))));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> hidden(component.create());
        QScopedPointer<QObject> current(component.create());
        QVERIFY(hidden);
        QVERIFY(current);

        FakeWidgetContext hiddenContext;
        FakeWidgetContext currentContext;
        currentContext.setIsCurrentPage(true);
        QVERIFY(hidden->setProperty(
            "widgetContext", QVariant::fromValue<QObject*>(&hiddenContext)));
        QVERIFY(current->setProperty(
            "widgetContext", QVariant::fromValue<QObject*>(&currentContext)));
        QTRY_VERIFY(current->property("ownsSink").toBool());
        QVERIFY(!hidden->property("ownsSink").toBool());
        QVERIFY(display.sink());

        // Exercise the adverse ordering: the incoming page becomes current
        // before the outgoing page releases the singleton sink.
        hiddenContext.setIsCurrentPage(true);
        currentContext.setIsCurrentPage(false);
        QTRY_VERIFY_WITH_TIMEOUT(hidden->property("ownsSink").toBool(), 1000);
        QVERIFY(!current->property("ownsSink").toBool());
        QVERIFY(display.sink());

        auto* crop = hidden->findChild<QQuickItem*>("clusterCropViewport");
        auto* video = hidden->findChild<QQuickItem*>("clusterVideoOutput");
        QVERIFY(crop);
        QVERIFY(video);
        QVERIFY(crop->clip());

        for (const qreal side : {300.0, 450.0}) {
            hidden->setProperty("width", side);
            hidden->setProperty("height", side);
            QCoreApplication::processEvents();
            const qreal scale = side / 300.0;
            QCOMPARE(crop->width(), side);
            QCOMPARE(crop->height(), side);
            QCOMPARE(video->width(), 800.0 * scale);
            QCOMPARE(video->height(), 480.0 * scale);
            QCOMPARE(video->x(), -250.0 * scale);
            QCOMPARE(video->y(), -90.0 * scale);
        }

        hidden->setProperty("width", 600.0);
        hidden->setProperty("height", 400.0);
        QCoreApplication::processEvents();
        const qreal scale = 400.0 / 300.0;
        QCOMPARE(crop->width(), 400.0);
        QCOMPARE(crop->height(), 400.0);
        QCOMPARE(crop->height(), crop->width());
        QCOMPARE(crop->x(), 100.0);
        QCOMPARE(crop->y(), 0.0);
        QCOMPARE(video->width(), 800.0 * scale);
        QCOMPARE(video->height(), 480.0 * scale);
        QCOMPARE(video->x(), -250.0 * scale);
        QCOMPARE(video->y(), -90.0 * scale);

        display.setGeometry({1280, 720, 600, 400});
        hidden->setProperty("width", 400.0);
        hidden->setProperty("height", 400.0);
        QCoreApplication::processEvents();
        const qreal rectangularScale = 400.0 / 600.0;
        QCOMPARE(crop->width(), 400.0);
        QVERIFY(qAbs(crop->height() - 400.0 * 400.0 / 600.0) < 0.001);
        QCOMPARE(crop->x(), 0.0);
        QVERIFY(qAbs(crop->y() - (400.0 - crop->height()) / 2.0) < 1.0);
        QVERIFY(qAbs(video->width() - 1280.0 * rectangularScale) < 0.001);
        QCOMPARE(video->height(), 720.0 * rectangularScale);
        QVERIFY(qAbs(video->x() - -340.0 * rectangularScale) < 0.001);
        QVERIFY(qAbs(video->y() - -160.0 * rectangularScale) < 0.001);
    }

    void failedSinkClaimStopsAfterBoundAndRearmsWhenOwnerReleases()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto writeFile = [&dir](const QString& name,
                                      const QByteArray& contents) {
            QSaveFile file(dir.filePath(name));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                return false;
            if (file.write(contents) != contents.size())
                return false;
            return file.commit();
        };
        QVERIFY(writeFile(QStringLiteral("AAClusterWidget.qml"),
                          qmlSource().toUtf8()));
        QVERIFY(writeFile(
            QStringLiteral("MaterialIcon.qml"),
            QByteArrayLiteral(
                "import QtQuick\nItem { property string icon; property real "
                "size; property color color; implicitWidth: size; "
                "implicitHeight: size }\n")));
        QVERIFY(writeFile(QStringLiteral("NormalText.qml"),
                          QByteArrayLiteral("import QtQuick\nText {}\n")));

        FakeClusterDisplay display;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(
            QStringLiteral("AAClusterDisplay"), &display);
        engine.rootContext()->setContextProperty(
            QStringLiteral("UiMetrics"),
            QVariantMap{{QStringLiteral("spacing"), 8},
                        {QStringLiteral("iconSize"), 24},
                        {QStringLiteral("fontBody"), 16}});
        engine.rootContext()->setContextProperty(
            QStringLiteral("ThemeService"),
            QVariantMap{{QStringLiteral("onSurfaceVariant"),
                         QStringLiteral("#ffffff")}});

        QQmlComponent component(
            &engine, QUrl::fromLocalFile(
                         dir.filePath(QStringLiteral("AAClusterWidget.qml"))));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> owner(component.create());
        QScopedPointer<QObject> blocked(component.create());
        QVERIFY(owner);
        QVERIFY(blocked);

        FakeWidgetContext ownerContext;
        FakeWidgetContext blockedContext;
        ownerContext.setIsCurrentPage(true);
        blockedContext.setIsCurrentPage(true);
        QVERIFY(owner->setProperty(
            "widgetContext", QVariant::fromValue<QObject*>(&ownerContext)));
        QVERIFY(blocked->setProperty(
            "widgetContext", QVariant::fromValue<QObject*>(&blockedContext)));
        QTRY_VERIFY(owner->property("ownsSink").toBool());

        const int maxAttempts =
            blocked->property("maxSinkClaimAttempts").toInt();
        QVERIFY(maxAttempts > 0);
        QTRY_COMPARE_WITH_TIMEOUT(
            blocked->property("sinkClaimAttempts").toInt(), maxAttempts,
            4000);
        const int attemptsAtBound = display.attachAttempts();
        QTest::qWait(500);
        QCOMPARE(display.attachAttempts(), attemptsAtBound);

        owner.reset();
        QTRY_VERIFY(blocked->property("ownsSink").toBool());
        QVERIFY(display.attachAttempts() > attemptsAtBound);
    }
};

QTEST_MAIN(TestAAClusterWidget)
#include "test_aa_cluster_widget.moc"
