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

        if (fileName == QStringLiteral("NavigationManeuverGlyph.qml")) {
            const QString directionSource =
                qmlSource(QStringLiteral("NavigationLaneDirectionGlyph.qml"));
            if (directionSource.isEmpty()
                || !writeFile(dir,
                              QStringLiteral("NavigationLaneDirectionGlyph.qml"),
                              directionSource.toUtf8())) {
                return nullptr;
            }
        }

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

        const QList<QVariantMap> defined{
            {{"type", 0}, {"lane", false}, {"token", ""},
             {"family", "unknown"}, {"mirrored", false}},
            {{"type", 1}, {"lane", false}, {"token", ""},
             {"family", "depart"}, {"mirrored", false}},
            {{"type", 2}, {"lane", true}, {"token", "straight"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 3}, {"lane", false}, {"token", ""},
             {"family", "keep"}, {"mirrored", true}},
            {{"type", 4}, {"lane", false}, {"token", ""},
             {"family", "keep"}, {"mirrored", false}},
            {{"type", 5}, {"lane", true}, {"token", "slight_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 6}, {"lane", true}, {"token", "slight_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 7}, {"lane", true}, {"token", "normal_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 8}, {"lane", true}, {"token", "normal_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 9}, {"lane", true}, {"token", "sharp_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 10}, {"lane", true}, {"token", "sharp_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 11}, {"lane", true}, {"token", "u_turn_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 12}, {"lane", true}, {"token", "u_turn_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 13}, {"lane", true}, {"token", "slight_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 14}, {"lane", true}, {"token", "slight_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 15}, {"lane", true}, {"token", "normal_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 16}, {"lane", true}, {"token", "normal_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 17}, {"lane", true}, {"token", "sharp_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 18}, {"lane", true}, {"token", "sharp_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 19}, {"lane", true}, {"token", "u_turn_left"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 20}, {"lane", true}, {"token", "u_turn_right"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 21}, {"lane", false}, {"token", ""},
             {"family", "off_ramp_slight"}, {"mirrored", true}},
            {{"type", 22}, {"lane", false}, {"token", ""},
             {"family", "off_ramp_slight"}, {"mirrored", false}},
            {{"type", 23}, {"lane", false}, {"token", ""},
             {"family", "off_ramp_normal"}, {"mirrored", true}},
            {{"type", 24}, {"lane", false}, {"token", ""},
             {"family", "off_ramp_normal"}, {"mirrored", false}},
            {{"type", 25}, {"lane", false}, {"token", ""},
             {"family", "fork"}, {"mirrored", true}},
            {{"type", 26}, {"lane", false}, {"token", ""},
             {"family", "fork"}, {"mirrored", false}},
            {{"type", 27}, {"lane", false}, {"token", ""},
             {"family", "merge"}, {"mirrored", true}},
            {{"type", 28}, {"lane", false}, {"token", ""},
             {"family", "merge"}, {"mirrored", false}},
            {{"type", 29}, {"lane", false}, {"token", ""},
             {"family", "merge_unspecified"}, {"mirrored", false}},
            {{"type", 32}, {"lane", false}, {"token", ""},
             {"family", "roundabout_enter_exit"}, {"mirrored", false}},
            {{"type", 33}, {"lane", false}, {"token", ""},
             {"family", "roundabout_enter_exit_angle"},
             {"mirrored", false}},
            {{"type", 34}, {"lane", false}, {"token", ""},
             {"family", "roundabout_enter_exit"}, {"mirrored", true}},
            {{"type", 35}, {"lane", false}, {"token", ""},
             {"family", "roundabout_enter_exit_angle"},
             {"mirrored", true}},
            {{"type", 36}, {"lane", true}, {"token", "straight"},
             {"family", ""}, {"mirrored", false}},
            {{"type", 37}, {"lane", false}, {"token", ""},
             {"family", "ferry_boat"}, {"mirrored", false}},
            {{"type", 38}, {"lane", false}, {"token", ""},
             {"family", "ferry_train"}, {"mirrored", false}},
            {{"type", 39}, {"lane", false}, {"token", ""},
             {"family", "destination"}, {"mirrored", false}},
            {{"type", 40}, {"lane", false}, {"token", ""},
             {"family", "destination"}, {"mirrored", false}},
            {{"type", 41}, {"lane", false}, {"token", ""},
             {"family", "destination_side"}, {"mirrored", true}},
            {{"type", 42}, {"lane", false}, {"token", ""},
             {"family", "destination_side"}, {"mirrored", false}},
            {{"type", 43}, {"lane", false}, {"token", ""},
             {"family", "roundabout_enter"}, {"mirrored", false}},
            {{"type", 44}, {"lane", false}, {"token", ""},
             {"family", "roundabout_exit"}, {"mirrored", false}},
            {{"type", 45}, {"lane", false}, {"token", ""},
             {"family", "roundabout_enter"}, {"mirrored", true}},
            {{"type", 46}, {"lane", false}, {"token", ""},
             {"family", "roundabout_exit"}, {"mirrored", true}},
            {{"type", 47}, {"lane", true}, {"token", "normal_left"},
             {"family", "ferry_boat_marker"}, {"mirrored", true}},
            {{"type", 48}, {"lane", true}, {"token", "normal_right"},
             {"family", "ferry_boat_marker"}, {"mirrored", false}},
            {{"type", 49}, {"lane", true}, {"token", "normal_left"},
             {"family", "ferry_train_marker"}, {"mirrored", true}},
            {{"type", 50}, {"lane", true}, {"token", "normal_right"},
             {"family", "ferry_train_marker"}, {"mirrored", false}},
        };

        auto* lanePrimitive = glyph->findChild<QQuickItem*>(
            QStringLiteral("maneuverLanePrimitive"));
        auto* specialCanvas = glyph->findChild<QQuickItem*>(
            QStringLiteral("maneuverSpecialCanvas"));
        QVERIFY(lanePrimitive);
        QVERIFY(specialCanvas);

        for (const QVariantMap& expected : defined) {
            const int value = expected.value("type").toInt();
            QVERIFY(glyph->setProperty("maneuverType", value));
            QVERIFY2(!glyph->property("isFallback").toBool(),
                     qPrintable(QStringLiteral(
                         "Defined maneuver %1 used the fallback").arg(value)));
            QCOMPARE(glyph->property("usesLanePrimitive").toBool(),
                     expected.value("lane").toBool());
            QCOMPARE(glyph->property("laneShapeToken").toString(),
                     expected.value("token").toString());
            QCOMPARE(glyph->property("specialFamily").toString(),
                     expected.value("family").toString());
            QCOMPARE(glyph->property("mirrored").toBool(),
                     expected.value("mirrored").toBool());
            const bool hasSpecial =
                !expected.value("family").toString().isEmpty();
            QCOMPARE(glyph->property("hasSpecialOverlay").toBool(),
                     hasSpecial);
            QCOMPARE(specialCanvas->isVisible(), hasSpecial);

            if (expected.value("lane").toBool()) {
                QVERIFY(lanePrimitive->isVisible());
                QCOMPARE(lanePrimitive->property("shapeToken").toString(),
                         expected.value("token").toString());
                QVERIFY(lanePrimitive->property("recommended").toBool());
                QCOMPARE(lanePrimitive->property("opticalScale").toReal(),
                         1.0);
            } else {
                QVERIFY(!lanePrimitive->isVisible());
            }
        }

        for (const int value : {30, 31, -1, 999}) {
            QVERIFY(glyph->setProperty("maneuverType", value));
            QVERIFY2(glyph->property("isFallback").toBool(),
                     qPrintable(QStringLiteral(
                         "Undefined maneuver %1 did not fall back").arg(value)));
            QVERIFY(!glyph->property("usesLanePrimitive").toBool());
            QCOMPARE(glyph->property("laneShapeToken").toString(),
                     QString());
            QCOMPARE(glyph->property("specialFamily").toString(),
                     QStringLiteral("unknown"));
            QVERIFY(!glyph->property("mirrored").toBool());
        }

        const QString source =
            qmlSource(QStringLiteral("NavigationManeuverGlyph.qml"));
        QVERIFY(!source.contains(QStringLiteral("MaterialIcon")));
        QCOMPARE(glyph->property("specialStrokeWidth").toReal(),
                 lanePrimitive->property("effectiveStrokeWidth").toReal());
        QCOMPARE(glyph->property("specialLineCap").toString(),
                 lanePrimitive->property("effectiveLineCap").toString());
        QCOMPARE(glyph->property("specialLineJoin").toString(),
                 lanePrimitive->property("effectiveLineJoin").toString());
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
        auto* stateText = widget->findChild<QQuickItem*>(
            QStringLiteral("navigationStateText"));
        auto* guidanceContent = widget->findChild<QQuickItem*>(
            QStringLiteral("guidanceContent"));
        auto* band = widget->findChild<QQuickItem*>(
            QStringLiteral("laneGuidanceBand"));
        auto* maneuver = widget->findChild<QQuickItem*>(
            QStringLiteral("maneuverGlyph"));
        auto* distance =
            widget->findChild<QQuickItem*>(QStringLiteral("distanceText"));
        auto* distanceUnit = widget->findChild<QQuickItem*>(
            QStringLiteral("distanceUnitText"));
        auto* road = widget->findChild<QQuickItem*>(QStringLiteral("roadText"));
        auto* secondaryCue = widget->findChild<QQuickItem*>(
            QStringLiteral("secondaryCueText"));
        auto* stepTime = widget->findChild<QQuickItem*>(
            QStringLiteral("stepTimeText"));
        auto* nextLabel =
            widget->findChild<QQuickItem*>(QStringLiteral("nextLabel"));
        auto* primaryTopRow = widget->findChild<QQuickItem*>(
            QStringLiteral("primaryTopRow"));
        auto* maneuverTile = widget->findChild<QQuickItem*>(
            QStringLiteral("maneuverTile"));
        auto* topInfoBlock = widget->findChild<QQuickItem*>(
            QStringLiteral("topInfoBlock"));
        auto* distanceRow = widget->findChild<QQuickItem*>(
            QStringLiteral("distanceRow"));
        auto* primaryGuidance = widget->findChild<QQuickItem*>(
            QStringLiteral("primaryGuidance"));
        auto* destinationFooter = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationFooter"));
        auto* destinationMetricRow = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationMetricRow"));
        auto* destinationPin = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationPin"));
        auto* destinationDistance = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationDistanceText"));
        auto* destinationEta = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationEtaText"));
        auto* destinationDuration = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationDurationText"));
        auto* destinationLabel = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationLabel"));
        auto* destinationViewport = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationTextViewport"));
        auto* destinationText = widget->findChild<QQuickItem*>(
            QStringLiteral("destinationText"));
        auto* destinationMarquee = widget->findChild<QObject*>(
            QStringLiteral("destinationMarquee"));
        QVERIFY(stateText);
        QVERIFY(guidanceContent);
        QVERIFY(band);
        QVERIFY(maneuver);
        QVERIFY(distance);
        QVERIFY(distanceUnit);
        QVERIFY(road);
        QVERIFY(secondaryCue);
        QVERIFY(stepTime);
        QVERIFY(!nextLabel);
        QVERIFY(primaryTopRow);
        QVERIFY(maneuverTile);
        QVERIFY(topInfoBlock);
        QVERIFY(distanceRow);
        QVERIFY(primaryGuidance);
        QVERIFY(destinationFooter);
        QVERIFY(destinationMetricRow);
        QVERIFY(destinationPin);
        QVERIFY(destinationDistance);
        QVERIFY(destinationEta);
        QVERIFY(destinationDuration);
        QVERIFY(destinationLabel);
        QVERIFY(destinationViewport);
        QVERIFY(destinationText);
        QVERIFY(destinationMarquee);

        QCOMPARE(stateText->property("text").toString(),
                 QStringLiteral("Connect Android Auto"));
        QVERIFY(!guidanceContent->isVisible());

        projection.setProjectionState(3);
        QTRY_COMPARE(stateText->property("text").toString(),
                     QStringLiteral("Start a route in Android Auto"));

        emit handler.navigationStateSnapshotChanged(
            oaa::hu::NavigationState::Rerouting);
        QTRY_COMPARE(stateText->property("text").toString(),
                     QStringLiteral("Finding a new route"));
        QVERIFY(!guidanceContent->isVisible());
        QVERIFY(!maneuver->isVisible());
        QVERIFY(!road->isVisible());
        QVERIFY(!band->isVisible());
        QVERIFY(!destinationFooter->isVisible());
        QVERIFY(stateText->property("text").toString()
                    != QStringLiteral("Start a route in Android Auto"));

        emit handler.navigationStateSnapshotChanged(
            oaa::hu::NavigationState::Active);
        QTRY_COMPARE(stateText->property("text").toString(),
                     QStringLiteral("Finding a new route"));
        QVERIFY(!guidanceContent->isVisible());

        oaa::hu::NavigationPositionSnapshot position;
        position.hasStepDistance = true;
        position.stepDistance.hasValue = true;
        position.stepDistance.value = 250;
        position.stepDistance.hasDisplayText = true;
        position.stepDistance.displayText = QStringLiteral("250");
        position.stepDistance.hasUnit = true;
        position.stepDistance.unit = 1;
        position.hasTimeToStep = true;
        position.timeToStepSeconds = 300;
        oaa::hu::NavigationDestinationDistanceData destinationMetrics;
        destinationMetrics.hasDistance = true;
        destinationMetrics.distance.hasValue = true;
        destinationMetrics.distance.value = 12000;
        destinationMetrics.distance.hasDisplayText = true;
        destinationMetrics.distance.displayText = QStringLiteral("12");
        destinationMetrics.distance.hasUnit = true;
        destinationMetrics.distance.unit = 4;
        destinationMetrics.hasEstimatedTimeOfArrival = true;
        destinationMetrics.estimatedTimeOfArrival = QStringLiteral("4:42 PM");
        destinationMetrics.hasTimeToArrival = true;
        destinationMetrics.timeToArrivalSeconds = 1500;
        position.destinationDistances.append(destinationMetrics);
        position.hasCurrentRoad = true;
        position.currentRoad = QStringLiteral("Current Road");
        emit handler.navigationPositionChanged(position);
        QTRY_COMPARE(stateText->property("text").toString(),
                     QStringLiteral("Finding a new route"));
        QVERIFY(!guidanceContent->isVisible());

        oaa::hu::NavigationNotificationSnapshot notification;
        notification.stepCount = 1;
        notification.hasManeuver = true;
        notification.maneuverType = 8;
        notification.hasUpcomingRoad = true;
        notification.upcomingRoad = QStringLiteral("Main Street");
        notification.actionCues = {QStringLiteral("Take Exit 12")};
        emit handler.navigationNotificationChanged(notification);
        QTRY_VERIFY(guidanceContent->isVisible());
        QTRY_VERIFY(maneuver->isVisible());
        emit handler.navigationPositionChanged(position);
        QTRY_VERIFY(distance->isVisible());
        QVERIFY(road->isVisible());
        QVERIFY(!band->isVisible());
        QVERIFY(!destinationFooter->isVisible());
        QCOMPARE(destinationText->property("text").toString(), QString());
        const qreal expandedPrimaryHeight = primaryGuidance->height();

        QCOMPARE(secondaryCue->property("text").toString(),
                 QStringLiteral("Take Exit 12"));
        QCOMPARE(stepTime->property("text").toString(), QStringLiteral("5 min"));
        QVERIFY(secondaryCue->isVisible());
        QVERIFY(stepTime->isVisible());
        QCOMPARE(secondaryCue->property("horizontalAlignment").toInt(),
                 static_cast<int>(Qt::AlignRight));
        QCOMPARE(stepTime->property("horizontalAlignment").toInt(),
                 static_cast<int>(Qt::AlignRight));
        QVERIFY(secondaryCue->x() + secondaryCue->width() <= stepTime->x());

        const QString shortDestination = QStringLiteral("Civic Center");
        notification.destinations = {shortDestination};
        emit handler.navigationNotificationChanged(notification);
        QTRY_VERIFY(destinationFooter->isVisible());
        QVERIFY(!band->isVisible());
        QCOMPARE(destinationText->property("text").toString(),
                 shortDestination);
        QVERIFY(primaryGuidance->height() < expandedPrimaryHeight);
        QVERIFY(destinationViewport->property("clip").toBool());
        QVERIFY(!destinationViewport->property("overflow").toBool());
        QCOMPARE(destinationText->x(), 0.0);
        QVERIFY(!destinationMarquee->property("running").toBool());
        QVERIFY(destinationMetricRow->isVisible());
        QVERIFY(destinationPin->isVisible());
        QCOMPARE(destinationDistance->property("text").toString(),
                 QStringLiteral("12 mi"));
        QCOMPARE(destinationEta->property("text").toString(),
                 QStringLiteral("4:42 PM"));
        QCOMPARE(destinationDuration->property("text").toString(),
                 QStringLiteral("25 min"));
        QVERIFY(destinationDistance->isVisible());
        QVERIFY(destinationEta->isVisible());
        QVERIFY(destinationDuration->isVisible());
        QVERIFY(destinationLabel->isVisible());
        QVERIFY(destinationMetricRow->y() < destinationViewport->y());

        const auto verifyMetricFontFloor = [&]() {
            QCOMPARE(destinationDistance->property("font").value<QFont>().pixelSize(),
                     22);
            QCOMPARE(destinationEta->property("font").value<QFont>().pixelSize(),
                     22);
            QCOMPARE(destinationDuration->property("font").value<QFont>().pixelSize(),
                     22);
        };
        verifyMetricFontFloor();

        const QString longDestination = QStringLiteral(
            "North Regional Transportation Center, Concourse Seven, "
            "Passenger Entrance");
        notification.destinations = {longDestination};
        emit handler.navigationNotificationChanged(notification);
        QTRY_COMPARE(destinationText->property("text").toString(),
                     longDestination);
        QTRY_VERIFY(destinationViewport->property("overflow").toBool());
        QVERIFY(destinationText->implicitWidth()
                > destinationViewport->width());
        QVERIFY(destinationMarquee->property("running").toBool());
        QCOMPARE(destinationViewport->property("marqueeDwellMs").toInt(),
                 2000);
        QVERIFY(destinationViewport->property("marqueePixelsPerSecond").toReal()
                >= 20.0);
        QVERIFY(destinationViewport->property("marqueePixelsPerSecond").toReal()
                <= 30.0);

        widget->setProperty("width", 430.0);
        widget->setProperty("height", 364.0);
        QCoreApplication::processEvents();
        QVERIFY(destinationDistance->isVisible());
        QVERIFY(destinationEta->isVisible());
        QVERIFY(!destinationDuration->isVisible());
        QVERIFY(destinationLabel->isVisible());
        QVERIFY(destinationPin->isVisible());
        verifyMetricFontFloor();

        widget->setProperty("width", 364.0);
        widget->setProperty("height", 364.0);
        QCoreApplication::processEvents();
        QVERIFY(destinationDistance->isVisible());
        QVERIFY(destinationEta->isVisible());
        QVERIFY(!destinationDuration->isVisible());
        QVERIFY(!destinationLabel->isVisible());
        QVERIFY(destinationPin->isVisible());
        verifyMetricFontFloor();

        notification.actionCues = {QStringLiteral(
            "Keep right toward the convention center transportation district")};
        emit handler.navigationNotificationChanged(notification);
        QTRY_COMPARE(secondaryCue->property("text").toString(),
                     notification.actionCues.first());
        QCOMPARE(secondaryCue->property("font").value<QFont>().pixelSize(), 24);
        QVERIFY(secondaryCue->isVisible());
        QVERIFY(!stepTime->isVisible());

        notification.actionCues.clear();
        emit handler.navigationNotificationChanged(notification);
        QTRY_COMPARE(secondaryCue->property("text").toString(),
                     QStringLiteral("Next turn"));
        QCOMPARE(maneuverTile->height(), topInfoBlock->height());
        QVERIFY(maneuver->width() <= maneuverTile->width());
        QVERIFY(maneuver->height() <= maneuverTile->height());
        QCOMPARE(road->x(), 0.0);
        QCOMPARE(road->width(), primaryGuidance->width());
        QVERIFY(road->height()
                >= road->property("font").value<QFont>().pixelSize());

        widget->setProperty("width", 1024.0);
        widget->setProperty("height", 600.0);
        QCoreApplication::processEvents();

        notification.lanes = {
            {{{1, false}, {5, true}}},
            {{{5, true}}},
        };
        emit handler.navigationNotificationChanged(notification);
        QTRY_VERIFY(band->isVisible());
        QVERIFY(!destinationFooter->isVisible());
        QVERIFY(!destinationMetricRow->isVisible());
        QVERIFY(!destinationPin->isVisible());
        QVERIFY(!destinationDistance->isVisible());
        QVERIFY(!destinationEta->isVisible());
        QVERIFY(!destinationDuration->isVisible());
        QVERIFY(!destinationLabel->isVisible());
        QVERIFY(!destinationViewport->isVisible());
        QVERIFY(!destinationText->isVisible());
        QVERIFY(!destinationMarquee->property("running").toBool());
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
        QCOMPARE(composites[0]->property("opticalScale").toReal(), 0.90);
        QCOMPARE(composites[1]->property("opticalScale").toReal(), 1.0);

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
            QCOMPARE(primitive->property("opticalScale").toReal(), 0.90);
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
        const QList<QQuickItem*> secondLanePrimitives =
            visualItemsNamed(composites[1],
                             QStringLiteral("laneDirectionPrimitive"));
        QCOMPARE(secondLanePrimitives.size(), 1);
        QCOMPARE(secondLanePrimitives.first()->property("opticalScale").toReal(),
                 1.0);
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
        QCOMPARE(maneuverTile->parentItem(), primaryTopRow);
        QCOMPARE(topInfoBlock->parentItem(), primaryTopRow);
        QCOMPARE(road->parentItem(), primaryGuidance);
        QCOMPARE(maneuverTile->height(), primaryTopRow->height());
        QCOMPARE(topInfoBlock->height(), primaryTopRow->height());
        const qreal maneuverTileRatio =
            maneuverTile->width() / primaryGuidance->width();
        QVERIFY(maneuverTileRatio >= 0.33);
        QVERIFY(maneuverTileRatio <= 0.38);
        QVERIFY(maneuver->width() >= 125.0);
        QVERIFY(maneuver->width() <= maneuverTile->width());
        QVERIFY(maneuver->height() <= maneuverTile->height());
        QCOMPARE(road->x(), 0.0);
        QCOMPARE(road->width(), primaryGuidance->width());
        QCOMPARE(road->property("horizontalAlignment").toInt(),
                 static_cast<int>(Qt::AlignHCenter));
        QCOMPARE(secondaryCue->property("horizontalAlignment").toInt(),
                 static_cast<int>(Qt::AlignRight));
        QVERIFY(qAbs(distanceRow->x() + distanceRow->width()
                     - topInfoBlock->width()) < 0.01);
        QCOMPARE(distance->property("text").toString(), QStringLiteral("250"));
        const int distancePixels =
            distance->property("font").value<QFont>().pixelSize();
        QVERIFY(distancePixels >= 92);
        QVERIFY(distancePixels <= 100);
        QCOMPARE(distanceUnit->property("font").value<QFont>().pixelSize(), 38);
        QCOMPARE(road->property("text").toString(), QStringLiteral("Main Street"));
        QCOMPARE(road->property("font").value<QFont>().pixelSize(), 40);
        QCOMPARE(secondaryCue->property("font").value<QFont>().pixelSize(), 28);

        projection.setProjectionState(4);
        QTRY_VERIFY(maneuver->isVisible());

        notification.lanes.clear();
        emit handler.navigationNotificationChanged(notification);
        QTRY_VERIFY(destinationFooter->isVisible());
        emit handler.navigationStateSnapshotChanged(
            oaa::hu::NavigationState::Inactive);
        QTRY_VERIFY(!destinationFooter->isVisible());
        QTRY_COMPARE(stateText->property("text").toString(),
                     QStringLiteral("Start a route in Android Auto"));
        QTRY_COMPARE(destinationText->property("text").toString(), QString());
        QCOMPARE(destinationText->x(), 0.0);
        QVERIFY(!destinationMarquee->property("running").toBool());
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
