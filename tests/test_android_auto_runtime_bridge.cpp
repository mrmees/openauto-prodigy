// tests/test_android_auto_runtime_bridge.cpp
#include <QtTest/QtTest>
#include "core/aa/AndroidAutoRuntimeBridge.hpp"
#include "core/services/IConfigService.hpp"

// Minimal mock config service for testing
class MockConfigService : public oap::IConfigService {
public:
    QVariant value(const QString& key) const override {
        return values_.value(key);
    }
    void setValue(const QString& key, const QVariant& v) override {
        values_[key] = v;
    }
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}
    void save() override {}

    QMap<QString, QVariant> values_;
};

// Minimal mock orchestrator with just the methods the bridge needs
class MockOrchestrator : public QObject {
    Q_OBJECT
public:
    void setDisplayDimensions(int w, int h) { dispW_ = w; dispH_ = h; }
    void setNavbarThickness(int t) { navThick_ = t; }

    int dispW_ = 0, dispH_ = 0;
    int navThick_ = 56;
};

class TestAndroidAutoRuntimeBridge : public QObject {
    Q_OBJECT
private slots:
    void testConstruction();
    void testNavbarThicknessDefault();
    void testNavbarThicknessWithDpi();
    void testNavbarThicknessWithScale();
    void testVideoResolutionDefault();
    void testVideoResolution1080p();
    void testVideoResolution480p();
    void testDisplayDimensionsDefault();
    void testTouchDeviceFromConfig();
    void testTouchDeviceAutoDetectEmpty();
};

void TestAndroidAutoRuntimeBridge::testConstruction() {
    oap::aa::AndroidAutoRuntimeBridge bridge;
    // Default state: no touch reader, no coord bridge
    QCOMPARE(bridge.touchReader(), nullptr);
    QCOMPARE(bridge.coordBridge(), nullptr);
}

void TestAndroidAutoRuntimeBridge::testNavbarThicknessDefault() {
    // Without DisplayInfo or DPI, should return 56 (base value)
    oap::aa::AndroidAutoRuntimeBridge bridge;
    QCOMPARE(bridge.computeNavbarThickness(0, 1.0), 56);
}

void TestAndroidAutoRuntimeBridge::testNavbarThicknessWithDpi() {
    oap::aa::AndroidAutoRuntimeBridge bridge;
    // DPI 160 = scale 1.0, so result = round(56 * 1.0 * 1.0) = 56
    QCOMPARE(bridge.computeNavbarThickness(160, 1.0), 56);
    // DPI 320 = scale 2.0, so result = round(56 * 2.0 * 1.0) = 112
    QCOMPARE(bridge.computeNavbarThickness(320, 1.0), 112);
}

void TestAndroidAutoRuntimeBridge::testNavbarThicknessWithScale() {
    oap::aa::AndroidAutoRuntimeBridge bridge;
    // DPI 160, globalScale 2.0 -> round(56 * 1.0 * 2.0) = 112
    QCOMPARE(bridge.computeNavbarThickness(160, 2.0), 112);
    // DPI 320, globalScale 0.5 -> round(56 * 2.0 * 0.5) = 56
    QCOMPARE(bridge.computeNavbarThickness(320, 0.5), 56);
}

void TestAndroidAutoRuntimeBridge::testVideoResolutionDefault() {
    MockConfigService cfg;
    auto [w, h] = oap::aa::AndroidAutoRuntimeBridge::resolveAAResolution(&cfg);
    QCOMPARE(w, 1280);
    QCOMPARE(h, 720);
}

void TestAndroidAutoRuntimeBridge::testVideoResolution1080p() {
    MockConfigService cfg;
    cfg.values_["video.resolution"] = "1080p";
    auto [w, h] = oap::aa::AndroidAutoRuntimeBridge::resolveAAResolution(&cfg);
    QCOMPARE(w, 1920);
    QCOMPARE(h, 1080);
}

void TestAndroidAutoRuntimeBridge::testVideoResolution480p() {
    MockConfigService cfg;
    cfg.values_["video.resolution"] = "480p";
    auto [w, h] = oap::aa::AndroidAutoRuntimeBridge::resolveAAResolution(&cfg);
    QCOMPARE(w, 800);
    QCOMPARE(h, 480);
}

void TestAndroidAutoRuntimeBridge::testDisplayDimensionsDefault() {
    oap::aa::AndroidAutoRuntimeBridge bridge;
    // Default display dimensions (no DisplayInfo set)
    QCOMPARE(bridge.displayWidth(), 1024);
    QCOMPARE(bridge.displayHeight(), 600);
}

void TestAndroidAutoRuntimeBridge::testTouchDeviceFromConfig() {
    MockConfigService cfg;
    cfg.values_["touch.device"] = "/dev/input/event99";
    oap::aa::AndroidAutoRuntimeBridge bridge;
    QString device = bridge.resolveTouchDevice(&cfg);
    QCOMPARE(device, "/dev/input/event99");
}

void TestAndroidAutoRuntimeBridge::testTouchDeviceAutoDetectEmpty() {
    // With no config, and no real hardware, auto-detect returns empty
    MockConfigService cfg;
    oap::aa::AndroidAutoRuntimeBridge bridge;
    QString device = bridge.resolveTouchDevice(&cfg);
    // On dev VM with no touchscreen, should be empty (auto-detect finds nothing)
    // The important thing is it doesn't crash and returns a valid QString
    QVERIFY(device.isEmpty() || QFile::exists(device));
}

QTEST_GUILESS_MAIN(TestAndroidAutoRuntimeBridge)
#include "test_android_auto_runtime_bridge.moc"
