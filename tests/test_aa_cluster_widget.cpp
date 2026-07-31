#include <algorithm>

#include <QColor>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>
#include <QVideoSink>
#include <QtMath>
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>

#include "core/aa/NavigationDataBridge.hpp"
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

class FakeConfigService : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE QVariant value(const QString&) const { return mode_; }
    void setMode(const QString& mode)
    {
        mode_ = mode;
        emit configChanged(QStringLiteral("video.secondary_display_content"),
                           mode_);
    }

signals:
    void configChanged(const QString& path, const QVariant& value);

private:
    QString mode_ = QStringLiteral("map");
};

class FakeProjectionStatus : public QObject {
    Q_OBJECT
    Q_PROPERTY(int projectionState READ projectionState WRITE setProjectionState
                   NOTIFY projectionStateChanged)

public:
    int projectionState() const { return state_; }
    void setProjectionState(int state)
    {
        if (state_ == state)
            return;
        state_ = state;
        emit projectionStateChanged();
    }

signals:
    void projectionStateChanged();

private:
    int state_ = 0;
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

    static bool writeRuntimeQmlFiles(const QTemporaryDir& dir)
    {
        for (const QString& fileName : {
                 QStringLiteral("AAClusterWidget.qml"),
                 QStringLiteral("NativeNavigationCard.qml"),
                 QStringLiteral("NavigationLaneGuidanceBand.qml"),
                 QStringLiteral("NavigationLaneCompositeGlyph.qml"),
                 QStringLiteral("NavigationManeuverGlyph.qml"),
                 QStringLiteral("NavigationLaneDirectionGlyph.qml")}) {
            const QString source = qmlSource(fileName);
            if (!source.isEmpty()
                && !writeFile(dir, fileName, source.toUtf8())) {
                return false;
            }
        }

        return writeFile(
                   dir, QStringLiteral("MaterialIcon.qml"),
                   QByteArrayLiteral(
                       "import QtQuick\nItem { property string icon; property "
                       "real size; property color color; property int weight; "
                       "implicitWidth: size; implicitHeight: size }\n"))
            && writeFile(dir, QStringLiteral("NormalText.qml"),
                         QByteArrayLiteral("import QtQuick\nText {}\n"));
    }

    static void exposeRuntimeContext(QQmlEngine& engine,
                                     FakeClusterDisplay& display,
                                     FakeConfigService& config,
                                     FakeProjectionStatus& projection,
                                     oap::aa::NavigationDataBridge& navigation)
    {
        engine.rootContext()->setContextProperty(
            QStringLiteral("AAClusterDisplay"), &display);
        engine.rootContext()->setContextProperty(QStringLiteral("ConfigService"),
                                                 &config);
        engine.rootContext()->setContextProperty(
            QStringLiteral("ProjectionStatus"), &projection);
        engine.rootContext()->setContextProperty(
            QStringLiteral("NavigationProvider"), &navigation);
        engine.rootContext()->setContextProperty(
            QStringLiteral("UiMetrics"),
            QVariantMap{{QStringLiteral("spacing"), 8},
                        {QStringLiteral("iconSize"), 24},
                        {QStringLiteral("fontBody"), 16}});
        engine.rootContext()->setContextProperty(
            QStringLiteral("ThemeService"),
            QVariantMap{
                {QStringLiteral("surfaceContainerHigh"),
                 QStringLiteral("#202124")},
                {QStringLiteral("surfaceContainerLow"),
                 QStringLiteral("#17181a")},
                {QStringLiteral("onSurface"), QStringLiteral("#ffffff")},
                {QStringLiteral("onSurfaceVariant"),
                 QStringLiteral("#c4c7c5")},
                {QStringLiteral("primary"), QStringLiteral("#8ab4f8")},
                {QStringLiteral("outlineVariant"),
                 QStringLiteral("#5f6368")}});
    }

    static QList<QQuickItem*> visualItemsNamed(QQuickItem* root,
                                               const QString& objectName)
    {
        QList<QQuickItem*> matches;
        if (!root)
            return matches;
        for (QQuickItem* child : root->childItems()) {
            if (child->objectName() == objectName)
                matches.append(child);
            matches.append(visualItemsNamed(child, objectName));
        }
        return matches;
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

        const QList<QVariantMap> defined{
            {{QStringLiteral("token"), QStringLiteral("unknown")},
             {QStringLiteral("family"), QStringLiteral("neutral")},
             {QStringLiteral("angle"), -1},
             {QStringLiteral("mirrored"), false}},
            {{QStringLiteral("token"), QStringLiteral("straight")},
             {QStringLiteral("family"), QStringLiteral("straight")},
             {QStringLiteral("angle"), 0},
             {QStringLiteral("mirrored"), false}},
            {{QStringLiteral("token"), QStringLiteral("slight_left")},
             {QStringLiteral("family"), QStringLiteral("slight")},
             {QStringLiteral("angle"), 45},
             {QStringLiteral("mirrored"), true}},
            {{QStringLiteral("token"), QStringLiteral("slight_right")},
             {QStringLiteral("family"), QStringLiteral("slight")},
             {QStringLiteral("angle"), 45},
             {QStringLiteral("mirrored"), false}},
            {{QStringLiteral("token"), QStringLiteral("normal_left")},
             {QStringLiteral("family"), QStringLiteral("normal")},
             {QStringLiteral("angle"), 90},
             {QStringLiteral("mirrored"), true}},
            {{QStringLiteral("token"), QStringLiteral("normal_right")},
             {QStringLiteral("family"), QStringLiteral("normal")},
             {QStringLiteral("angle"), 90},
             {QStringLiteral("mirrored"), false}},
            {{QStringLiteral("token"), QStringLiteral("sharp_left")},
             {QStringLiteral("family"), QStringLiteral("sharp")},
             {QStringLiteral("angle"), 135},
             {QStringLiteral("mirrored"), true}},
            {{QStringLiteral("token"), QStringLiteral("sharp_right")},
             {QStringLiteral("family"), QStringLiteral("sharp")},
             {QStringLiteral("angle"), 135},
             {QStringLiteral("mirrored"), false}},
            {{QStringLiteral("token"), QStringLiteral("u_turn_left")},
             {QStringLiteral("family"), QStringLiteral("u_turn")},
             {QStringLiteral("angle"), 180},
             {QStringLiteral("mirrored"), true}},
            {{QStringLiteral("token"), QStringLiteral("u_turn_right")},
             {QStringLiteral("family"), QStringLiteral("u_turn")},
             {QStringLiteral("angle"), 180},
             {QStringLiteral("mirrored"), false}},
        };
        for (const QVariantMap& expected : defined) {
            const QString token =
                expected.value(QStringLiteral("token")).toString();
            QVERIFY(glyph->setProperty("shapeToken", token));
            QVERIFY2(!glyph->property("isFallback").toBool(),
                     qPrintable(QStringLiteral(
                         "Defined lane token %1 used the fallback").arg(token)));
            QCOMPARE(glyph->property("primitiveFamily").toString(),
                     expected.value(QStringLiteral("family")).toString());
            QCOMPARE(glyph->property("semanticAngle").toInt(),
                     expected.value(QStringLiteral("angle")).toInt());
            QCOMPARE(glyph->property("mirrored").toBool(),
                     expected.value(QStringLiteral("mirrored")).toBool());
            QCOMPARE(glyph->property("drawNeutralStem").toBool(),
                     token == QStringLiteral("unknown"));

            if (token != QStringLiteral("unknown")) {
                const qreal terminalDx =
                    glyph->property("terminalDx").toReal();
                const qreal terminalDy =
                    glyph->property("terminalDy").toReal();
                const qreal terminalLength =
                    qSqrt(terminalDx * terminalDx + terminalDy * terminalDy);
                QVERIFY(terminalLength > 0.0);
                const qreal actualAngle = qRadiansToDegrees(
                    qAtan2(qAbs(terminalDx), -terminalDy));
                QVERIFY(qAbs(actualAngle
                             - expected.value(QStringLiteral("angle")).toReal())
                        < 0.01);

                const qreal headADx = glyph->property("headADx").toReal();
                const qreal headADy = glyph->property("headADy").toReal();
                const qreal headBDx = glyph->property("headBDx").toReal();
                const qreal headBDy = glyph->property("headBDy").toReal();
                const qreal headALength =
                    qSqrt(headADx * headADx + headADy * headADy);
                const qreal headBLength =
                    qSqrt(headBDx * headBDx + headBDy * headBDy);
                QVERIFY(headALength > 0.0);
                QVERIFY(headBLength > 0.0);
                const qreal includedAngle = qRadiansToDegrees(qAcos(
                    (headADx * headBDx + headADy * headBDy)
                    / (headALength * headBLength)));
                QVERIFY(qAbs(includedAngle - 70.0) < 0.05);

                const qreal bisectorX =
                    headADx / headALength + headBDx / headBLength;
                const qreal bisectorY =
                    headADy / headALength + headBDy / headBLength;
                const qreal bisectorLength =
                    qSqrt(bisectorX * bisectorX + bisectorY * bisectorY);
                const qreal alignment =
                    (bisectorX * -terminalDx + bisectorY * -terminalDy)
                    / (bisectorLength * terminalLength);
                QVERIFY(qAbs(alignment - 1.0) < 0.001);
            }
        }

        const QList<QVariantMap> variants{
            {{QStringLiteral("token"), QStringLiteral("straight")},
             {QStringLiteral("variant"), QStringLiteral("tall")},
             {QStringLiteral("anchor"), 0.5}},
            {{QStringLiteral("token"), QStringLiteral("slight_right")},
             {QStringLiteral("variant"), QStringLiteral("tall")},
             {QStringLiteral("anchor"), 0.25}},
            {{QStringLiteral("token"), QStringLiteral("normal_right")},
             {QStringLiteral("variant"), QStringLiteral("short")},
             {QStringLiteral("anchor"), 0.25}},
            {{QStringLiteral("token"), QStringLiteral("sharp_right")},
             {QStringLiteral("variant"), QStringLiteral("short")},
             {QStringLiteral("anchor"), 0.375}},
            {{QStringLiteral("token"), QStringLiteral("u_turn_right")},
             {QStringLiteral("variant"), QStringLiteral("short")},
             {QStringLiteral("anchor"), 0.375}},
        };
        for (const QVariantMap& expected : variants) {
            QVERIFY(glyph->setProperty(
                "shapeToken", expected.value(QStringLiteral("token"))));
            QVERIFY(glyph->setProperty(
                "geometryVariant", expected.value(QStringLiteral("variant"))));
            QCOMPARE(glyph->property("anchorFraction").toReal(),
                     expected.value(QStringLiteral("anchor")).toReal());
        }

        auto* directionVisual =
            glyph->findChild<QObject*>(QStringLiteral("laneDirectionGlyph"));
        QVERIFY(directionVisual);

        for (const QString& token : {QStringLiteral("unknown_future"),
                                     QStringLiteral("sideways")}) {
            QVERIFY(glyph->setProperty("shapeToken", token));
            QVERIFY(glyph->property("isFallback").toBool());
            QVERIFY(glyph->property("drawNeutralStem").toBool());
            QCOMPARE(glyph->property("primitiveFamily").toString(),
                     QStringLiteral("neutral"));
        }
    }

    void laneBandIsContinuousNonInteractiveAndDimsAtComponentLevel()
    {
        const QString source =
            qmlSource(QStringLiteral("NavigationLaneGuidanceBand.qml"));
        const QString compositeSource =
            qmlSource(QStringLiteral("NavigationLaneCompositeGlyph.qml"));
        const QString directionSource =
            qmlSource(QStringLiteral("NavigationLaneDirectionGlyph.qml"));
        QVERIFY2(!source.isEmpty(),
                 "NavigationLaneGuidanceBand.qml has not been implemented");
        QVERIFY2(!compositeSource.isEmpty(),
                 "NavigationLaneCompositeGlyph.qml has not been implemented");
        QVERIFY(source.contains(QStringLiteral("property var laneModel")));
        QVERIFY(compositeSource.contains(QStringLiteral(
            "opacity: direction.recommended ? 1.0 : 0.48")));
        QVERIFY(compositeSource.contains(QStringLiteral("ThemeService.primary")));
        QVERIFY(compositeSource.contains(
            QStringLiteral("ThemeService.onSurfaceVariant")));
        QVERIFY(directionSource.contains(QStringLiteral("Canvas {")));
        QVERIFY(!directionSource.contains(QStringLiteral("rotation:")));

        const QString allLaneSources = source + compositeSource + directionSource;
        for (const QString& forbidden : {
                 QStringLiteral("MouseArea"), QStringLiteral("TapHandler"),
                 QStringLiteral("Button {"), QStringLiteral("Flickable")}) {
            QVERIFY2(!allLaneSources.contains(forbidden),
                     qPrintable(QStringLiteral("Forbidden lane-band token: %1")
                                    .arg(forbidden)));
        }
    }

    void currentWidgetSwitchesLocallyBetweenMapAndNativeCard()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeRuntimeQmlFiles(dir));

        FakeClusterDisplay display;
        FakeConfigService config;
        FakeProjectionStatus projection;
        projection.setProjectionState(3);
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge navigation;
        navigation.connectToHandler(&handler);

        QQmlEngine engine;
        exposeRuntimeContext(engine, display, config, projection, navigation);
        QQmlComponent component(
            &engine, QUrl::fromLocalFile(
                         dir.filePath(QStringLiteral("AAClusterWidget.qml"))));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> widget(component.create());
        QVERIFY2(widget, qPrintable(component.errorString()));
        widget->setProperty("width", 1024.0);
        widget->setProperty("height", 600.0);

        FakeWidgetContext context;
        context.setIsCurrentPage(true);
        QVERIFY(widget->setProperty(
            "widgetContext", QVariant::fromValue<QObject*>(&context)));
        QTRY_VERIFY(widget->property("ownsSink").toBool());
        auto* video =
            widget->findChild<QQuickItem*>(QStringLiteral("clusterVideoOutput"));
        auto* card = widget->findChild<QQuickItem*>(
            QStringLiteral("nativeNavigationCard"));
        QVERIFY(video);
        QVERIFY(card);
        QVERIFY(!card->isVisible());

        config.setMode(QStringLiteral("turn_card"));
        QTRY_VERIFY(!widget->property("ownsSink").toBool());
        QVERIFY(!display.sink());
        QTRY_VERIFY(card->isVisible());
        QCOMPARE(widget->findChild<QQuickItem*>(
                     QStringLiteral("clusterVideoOutput")),
                 video);
        QCOMPARE(projection.projectionState(), 3);

        config.setMode(QStringLiteral("map"));
        QTRY_VERIFY(widget->property("ownsSink").toBool());
        QVERIFY(display.sink());
        QVERIFY(!card->isVisible());

        config.setMode(QStringLiteral("not-a-real-mode"));
        QTRY_COMPARE(widget->property("dashboardNavigationMode").toString(),
                     QStringLiteral("map"));
        QVERIFY(widget->property("ownsSink").toBool());
    }

    void nativeCardDistinguishesConnectionStatesAndShowsActiveLanes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeRuntimeQmlFiles(dir));

        FakeClusterDisplay display;
        FakeConfigService config;
        config.setMode(QStringLiteral("turn_card"));
        FakeProjectionStatus projection;
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge navigation;
        navigation.connectToHandler(&handler);

        QQmlEngine engine;
        exposeRuntimeContext(engine, display, config, projection, navigation);
        QQmlComponent component(
            &engine, QUrl::fromLocalFile(
                         dir.filePath(QStringLiteral("AAClusterWidget.qml"))));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> widget(component.create());
        QVERIFY2(widget, qPrintable(component.errorString()));
        widget->setProperty("width", 1024.0);
        widget->setProperty("height", 600.0);

        FakeWidgetContext context;
        context.setIsCurrentPage(true);
        QVERIFY(widget->setProperty(
            "widgetContext", QVariant::fromValue<QObject*>(&context)));
        auto* stateText =
            widget->findChild<QQuickItem*>(QStringLiteral("navigationStateText"));
        auto* band = widget->findChild<QQuickItem*>(
            QStringLiteral("laneGuidanceBand"));
        QVERIFY(stateText);
        QVERIFY(band);
        QCOMPARE(stateText->property("text").toString(),
                 QStringLiteral("Connect Android Auto"));

        projection.setProjectionState(3);
        QTRY_COMPARE(stateText->property("text").toString(),
                     QStringLiteral("Start a route in Android Auto"));

        emit handler.navigationStateChanged(true);
        auto* maneuver = widget->findChild<QQuickItem*>(
            QStringLiteral("maneuverGlyph"));
        auto* distance =
            widget->findChild<QQuickItem*>(QStringLiteral("distanceText"));
        auto* distanceUnit = widget->findChild<QQuickItem*>(
            QStringLiteral("distanceUnitText"));
        auto* road = widget->findChild<QQuickItem*>(QStringLiteral("roadText"));
        auto* secondaryCue = widget->findChild<QQuickItem*>(
            QStringLiteral("secondaryCueText"));
        auto* nextLabel =
            widget->findChild<QQuickItem*>(QStringLiteral("nextLabel"));
        QVERIFY(maneuver);
        QVERIFY(distance);
        QVERIFY(distanceUnit);
        QVERIFY(road);
        QVERIFY(secondaryCue);
        QVERIFY(nextLabel);
        QTRY_VERIFY(maneuver->isVisible());
        QVERIFY(!distance->isVisible());
        QVERIFY(!road->isVisible());
        QVERIFY(!band->isVisible());

        emit handler.navigationTurnEvent(QStringLiteral("Main Street"), 8, 1,
                                         QByteArray(), 250, 1);
        emit handler.navigationDistanceChanged(QStringLiteral("250"), 1);
        emit handler.navigationLaneGuidanceChanged({
            {{{1, false}, {5, true}}},
            {{{5, true}}},
        });
        QTRY_VERIFY(band->isVisible());
        QTRY_COMPARE(visualItemsNamed(
                         qobject_cast<QQuickItem*>(widget.data()),
                         QStringLiteral("laneComposite")).size(),
                     2);
        QList<QQuickItem*> composites = visualItemsNamed(
            qobject_cast<QQuickItem*>(widget.data()),
            QStringLiteral("laneComposite"));
        std::sort(composites.begin(), composites.end(),
                  [](const QQuickItem* left, const QQuickItem* right) {
                      return left->property("laneIndex").toInt()
                          < right->property("laneIndex").toInt();
                  });
        QCOMPARE(composites.size(), 2);
        QCOMPARE(composites[0]->property("directionCount").toInt(), 2);
        QCOMPARE(composites[1]->property("directionCount").toInt(), 1);

        const QList<QQuickItem*> primitives = visualItemsNamed(
            qobject_cast<QQuickItem*>(widget.data()),
            QStringLiteral("laneDirectionPrimitive"));
        QCOMPARE(primitives.size(), 3);
        const QList<QQuickItem*> dividers = visualItemsNamed(
            qobject_cast<QQuickItem*>(widget.data()),
            QStringLiteral("laneDivider"));
        QCOMPARE(dividers.size(), 1);

        const QList<QQuickItem*> firstLanePrimitives =
            visualItemsNamed(composites[0],
                             QStringLiteral("laneDirectionPrimitive"));
        QCOMPARE(firstLanePrimitives.size(), 2);
        QQuickItem* alternative = nullptr;
        QQuickItem* recommended = nullptr;
        for (QQuickItem* primitive : firstLanePrimitives) {
            if (primitive->property("recommended").toBool())
                recommended = primitive;
            else
                alternative = primitive;
        }
        QVERIFY(alternative);
        QVERIFY(recommended);
        QCOMPARE(alternative->x(), recommended->x());
        QCOMPARE(alternative->y(), recommended->y());
        QCOMPARE(alternative->width(), recommended->width());
        QCOMPARE(alternative->height(), recommended->height());
        QCOMPARE(alternative->property("shapeToken").toString(),
                 QStringLiteral("straight"));
        QCOMPARE(alternative->property("geometryVariant").toString(),
                 QStringLiteral("tall"));
        QCOMPARE(recommended->property("shapeToken").toString(),
                 QStringLiteral("normal_right"));
        QCOMPARE(recommended->property("geometryVariant").toString(),
                 QStringLiteral("short"));
        QVERIFY(recommended->z() > alternative->z());
        QCOMPARE(alternative->opacity(), 0.48);
        QCOMPARE(recommended->opacity(), 1.0);
        QCOMPARE(alternative->property("color").value<QColor>(),
                 QColor(QStringLiteral("#c4c7c5")));
        QCOMPARE(recommended->property("color").value<QColor>(),
                 QColor(QStringLiteral("#8ab4f8")));
        QVERIFY(maneuver->isVisible());
        QVERIFY(distance->isVisible());
        QVERIFY(road->isVisible());
        QCOMPARE(distance->property("text").toString(), QStringLiteral("250"));
        const int distancePixels =
            distance->property("font").value<QFont>().pixelSize();
        QVERIFY(distancePixels >= 92);
        QVERIFY(distancePixels <= 100);
        QCOMPARE(distanceUnit->property("font").value<QFont>().pixelSize(), 38);
        QCOMPARE(road->property("text").toString(), QStringLiteral("Main Street"));
        QCOMPARE(road->property("font").value<QFont>().pixelSize(), 40);
        QCOMPARE(secondaryCue->property("font").value<QFont>().pixelSize(), 28);
        QVERIFY(nextLabel->property("font").value<QFont>().pixelSize() >= 22);

        projection.setProjectionState(4);
        QTRY_VERIFY(maneuver->isVisible());
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
        QVERIFY(source.contains(QStringLiteral("Waiting for map")));
        QVERIFY(source.contains(QStringLiteral(
            "attachVideoSink(videoOutput.videoSink)")));
        QVERIFY(source.contains(QStringLiteral(
            "detachVideoSink(videoOutput.videoSink)")));
        QVERIFY(source.contains(QStringLiteral("widgetContext.isCurrentPage")));
        QVERIFY(source.contains(QStringLiteral("onIsCurrentPageChanged")));
        QVERIFY(source.contains(
            QStringLiteral("onVideoSinkAvailabilityChanged")));
        QVERIFY(source.contains(
            QStringLiteral("if (!isCurrentPage || !isMapMode)")));
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
        QVERIFY(writeRuntimeQmlFiles(dir));

        FakeClusterDisplay display;
        FakeConfigService config;
        FakeProjectionStatus projection;
        oap::aa::NavigationDataBridge navigation;
        QQmlEngine engine;
        exposeRuntimeContext(engine, display, config, projection, navigation);

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
        QVERIFY(writeRuntimeQmlFiles(dir));

        FakeClusterDisplay display;
        FakeConfigService config;
        FakeProjectionStatus projection;
        oap::aa::NavigationDataBridge navigation;
        QQmlEngine engine;
        exposeRuntimeContext(engine, display, config, projection, navigation);

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
