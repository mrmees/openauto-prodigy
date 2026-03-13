// tests/test_gesture_overlay_controller.cpp
#include <QtTest/QtTest>
#include <algorithm>
#include "ui/GestureOverlayController.hpp"

// Minimal mock AudioService
class MockAudioService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int masterVolume READ masterVolume WRITE setMasterVolume NOTIFY masterVolumeChanged)
public:
    explicit MockAudioService(QObject* parent = nullptr) : QObject(parent) {}
    int masterVolume() const { return vol_; }
    void setMasterVolume(int v) { vol_ = v; emit masterVolumeChanged(); }
signals:
    void masterVolumeChanged();
private:
    int vol_ = 80;
};

// Minimal mock DisplayService
class MockDisplayService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
public:
    explicit MockDisplayService(QObject* parent = nullptr) : QObject(parent) {}
    int brightness() const { return bright_; }
    void setBrightness(int b) { bright_ = b; emit brightnessChanged(); }
signals:
    void brightnessChanged();
private:
    int bright_ = 100;
};

class TestGestureOverlayController : public QObject {
    Q_OBJECT
private slots:
    void testConstruction();
    void testZoneNamesReturned();
    void testCleanupRemovesZones();
    void testServiceSetters();
    void testOverlayNotVisibleByDefault();
};

void TestGestureOverlayController::testConstruction() {
    oap::GestureOverlayController ctrl;
    // Should construct without crashing
    QVERIFY(!ctrl.isOverlayVisible());
}

void TestGestureOverlayController::testZoneNamesReturned() {
    oap::GestureOverlayController ctrl;
    // The controller should know the zone IDs it manages
    auto zones = ctrl.managedZoneIds();
    auto has = [&](const std::string& id) {
        return std::find(zones.begin(), zones.end(), id) != zones.end();
    };
    QVERIFY(has("gesture-overlay"));
    QVERIFY(has("overlay-volume"));
    QVERIFY(has("overlay-brightness"));
    QVERIFY(has("overlay-home"));
    QVERIFY(has("overlay-theme"));
    QVERIFY(has("overlay-close"));
    QCOMPARE(static_cast<int>(zones.size()), 6);
}

void TestGestureOverlayController::testCleanupRemovesZones() {
    oap::GestureOverlayController ctrl;
    // cleanup should be callable even without a bridge (no-op)
    ctrl.cleanupZones();
    QVERIFY(!ctrl.isOverlayVisible());
}

void TestGestureOverlayController::testServiceSetters() {
    oap::GestureOverlayController ctrl;
    MockAudioService audio;
    MockDisplayService display;
    // Should accept services without crashing
    ctrl.setAudioService(&audio);
    ctrl.setDisplayService(&display);
}

void TestGestureOverlayController::testOverlayNotVisibleByDefault() {
    oap::GestureOverlayController ctrl;
    QVERIFY(!ctrl.isOverlayVisible());
}

QTEST_GUILESS_MAIN(TestGestureOverlayController)
#include "test_gesture_overlay_controller.moc"
