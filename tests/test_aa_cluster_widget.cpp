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
    Q_PROPERTY(bool awaitingContent READ awaitingContent CONSTANT)
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
    bool awaitingContent() const { return true; }
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
    static QString qmlSource(const QString& fileName =
                                 QStringLiteral("AAClusterWidget.qml"))
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR "/qml/widgets/") + fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    }

    static bool writeFile(const QTemporaryDir& dir, const QString& name,
                          const QByteArray& contents)
    {
        QSaveFile file(dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        if (file.write(contents) != contents.size())
            return false;
        return file.commit();
    }

    static QQmlComponent* createGlyphComponent(QQmlEngine& engine,
                                                QTemporaryDir& dir,
                                                const QString& fileName)
    {
        const QByteArray materialIconStub = QByteArrayLiteral(
            "import QtQuick\n"
            "Item { property string icon; property real size; property color "
            "color; property int weight; implicitWidth: size; implicitHeight: "
            "size }\n");
        if (!writeFile(dir, QStringLiteral("MaterialIcon.qml"),
                       materialIconStub))
            return nullptr;

        const QString source = qmlSource(fileName);
        if (!source.isEmpty()
            && !writeFile(dir, fileName, source.toUtf8()))
            return nullptr;

        return new QQmlComponent(&engine,
                                 QUrl::fromLocalFile(dir.filePath(fileName)));
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

    void maneuverGlyphMapsEveryDefinedValueAndFallsBackSafely()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QQmlEngine engine;
        QScopedPointer<QQmlComponent> component(createGlyphComponent(
            engine, dir, QStringLiteral("NavigationManeuverGlyph.qml")));
        QVERIFY(component);
        QVERIFY2(component->isReady(), qPrintable(component->errorString()));
        QScopedPointer<QObject> glyph(component->create());
        QVERIFY2(glyph, qPrintable(component->errorString()));

        QList<int> defined;
        for (int value = 0; value <= 29; ++value)
            defined.append(value);
        for (int value = 32; value <= 50; ++value)
            defined.append(value);

        for (const int value : defined) {
            QVERIFY(glyph->setProperty("maneuverType", value));
            QVERIFY2(!glyph->property("isFallback").toBool(),
                     qPrintable(QStringLiteral(
                         "Defined maneuver %1 used the fallback").arg(value)));
            QVERIFY2(!glyph->property("primaryGlyph").toString().isEmpty(),
                     qPrintable(QStringLiteral(
                         "Defined maneuver %1 had no primary glyph").arg(value)));
        }

        for (const int value : {30, 31, -1, 999}) {
            QVERIFY(glyph->setProperty("maneuverType", value));
            QVERIFY2(glyph->property("isFallback").toBool(),
                     qPrintable(QStringLiteral(
                         "Undefined maneuver %1 did not fall back").arg(value)));
            QVERIFY(!glyph->property("primaryGlyph").toString().isEmpty());
        }

        const QList<QPair<int, int>> directionalPairs{
            {3, 4},   {5, 6},   {7, 8},   {9, 10},  {11, 12},
            {13, 14}, {15, 16}, {17, 18}, {19, 20}, {21, 22},
            {23, 24}, {25, 26}, {27, 28}, {32, 34}, {33, 35},
            {41, 42}, {43, 45}, {44, 46}, {47, 48}, {49, 50},
        };
        const auto signature = [&glyph](int value) {
            glyph->setProperty("maneuverType", value);
            return QStringLiteral("%1|%2|%3")
                .arg(glyph->property("primaryGlyph").toString(),
                     glyph->property("badgeGlyph").toString(),
                     glyph->property("mirrorPrimary").toBool()
                         ? QStringLiteral("mirrored")
                         : QStringLiteral("normal"));
        };
        for (const auto& pair : directionalPairs) {
            const QString left = signature(pair.first);
            const QString right = signature(pair.second);
            QVERIFY2(left != right,
                     qPrintable(QStringLiteral(
                         "Directional maneuvers %1 and %2 collapsed to %3")
                                    .arg(pair.first)
                                    .arg(pair.second)
                                    .arg(left)));
        }
    }

    void laneDirectionGlyphMapsStableTokensAndFallsBackSafely()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QQmlEngine engine;
        QScopedPointer<QQmlComponent> component(createGlyphComponent(
            engine, dir, QStringLiteral("NavigationLaneDirectionGlyph.qml")));
        QVERIFY(component);
        QVERIFY2(component->isReady(), qPrintable(component->errorString()));
        QScopedPointer<QObject> glyph(component->create());
        QVERIFY2(glyph, qPrintable(component->errorString()));

        const QStringList defined{
            QStringLiteral("unknown"),      QStringLiteral("straight"),
            QStringLiteral("slight_left"),  QStringLiteral("slight_right"),
            QStringLiteral("normal_left"),  QStringLiteral("normal_right"),
            QStringLiteral("sharp_left"),   QStringLiteral("sharp_right"),
            QStringLiteral("u_turn_left"),  QStringLiteral("u_turn_right"),
        };
        for (const QString& token : defined) {
            QVERIFY(glyph->setProperty("shapeToken", token));
            QVERIFY2(!glyph->property("isFallback").toBool(),
                     qPrintable(QStringLiteral(
                         "Defined lane token %1 used the fallback").arg(token)));
            if (token == QStringLiteral("unknown")) {
                QVERIFY(glyph->property("drawNeutralStem").toBool());
                QVERIFY(glyph->property("glyph").toString().isEmpty());
            } else {
                QVERIFY(!glyph->property("drawNeutralStem").toBool());
                QVERIFY2(!glyph->property("glyph").toString().isEmpty(),
                         qPrintable(QStringLiteral(
                             "Defined lane token %1 had no glyph").arg(token)));
            }
        }

        QVERIFY(glyph->setProperty("recommended", false));
        QVERIFY(glyph->setProperty("shapeToken", QStringLiteral("straight")));
        auto* directionVisual =
            glyph->findChild<QObject*>(QStringLiteral("laneDirectionGlyph"));
        QVERIFY(directionVisual);
        QCOMPARE(directionVisual->property("opacity").toReal(), 1.0);

        QVERIFY(glyph->setProperty("shapeToken", QStringLiteral("unknown")));
        auto* neutralStem =
            glyph->findChild<QObject*>(QStringLiteral("laneNeutralStem"));
        QVERIFY(neutralStem);
        QCOMPARE(neutralStem->property("opacity").toReal(), 1.0);

        for (const QString& token : {QStringLiteral("unknown_future"),
                                     QStringLiteral("sideways")}) {
            QVERIFY(glyph->setProperty("shapeToken", token));
            QVERIFY(glyph->property("isFallback").toBool());
            QVERIFY(glyph->property("drawNeutralStem").toBool());
            QVERIFY(glyph->property("glyph").toString().isEmpty());
        }
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
        QVERIFY(source.contains(QStringLiteral("AAClusterDisplay.awaitingContent")));
        QVERIFY(source.contains(QStringLiteral(
            "Start navigation to see turn-by-turn directions")));
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
