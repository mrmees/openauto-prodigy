#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include <array>
#include <memory>

namespace oap { namespace aa {
class EvdevCoordBridge;
enum class TouchEvent;
}}

namespace oap {

class ActionRegistry;

/// C++ gesture state machine for the 3-control navbar.
/// Receives touch events from QML (handlePress/Release/Cancel) or evdev zone
/// callbacks (onZoneTouch) and emits unified gesture signals.
class NavbarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString edge READ edge WRITE setEdge NOTIFY edgeChanged)
    Q_PROPERTY(bool leftHandDrive READ leftHandDrive WRITE setLeftHandDrive NOTIFY layoutChanged)
    Q_PROPERTY(bool popupVisible READ popupVisible NOTIFY popupChanged)
    Q_PROPERTY(int popupControlIndex READ popupControlIndex NOTIFY popupChanged)

public:
    /// Gesture types (exposed as int to QML)
    enum Gesture { Tap = 0, ShortHold = 1, LongHold = 2 };
    Q_ENUM(Gesture)

    explicit NavbarController(QObject* parent = nullptr);
    ~NavbarController() override;

    // --- QML input handlers (launcher mode) ---
    Q_INVOKABLE void handlePress(int controlIndex);
    Q_INVOKABLE void handleRelease(int controlIndex);
    Q_INVOKABLE void handleCancel(int controlIndex);

    // --- Evdev zone callback (reader thread) ---
    // Must marshal to main thread via QMetaObject::invokeMethod
    void onZoneTouch(int controlIndex, int slot, float x, float y,
                     oap::aa::TouchEvent event);

    // --- Properties ---
    QString edge() const;
    void setEdge(const QString& edge);

    bool leftHandDrive() const;
    void setLeftHandDrive(bool lhd);

    bool popupVisible() const;
    int popupControlIndex() const;

    // --- Popup management ---
    Q_INVOKABLE void showPopup(int controlIndex);
    Q_INVOKABLE void hidePopup();

    // --- Control role mapping ---
    /// Returns "volume", "clock", or "brightness" for the given controlIndex (0-2)
    Q_INVOKABLE QString controlRole(int controlIndex) const;

    // --- Gesture timing configuration ---
    int tapMaxMs() const;
    void setTapMaxMs(int ms);
    int shortHoldMaxMs() const;
    void setShortHoldMaxMs(int ms);

    // --- Zone registration ---
    Q_INVOKABLE void registerZones(int displayWidth, int displayHeight);
    Q_INVOKABLE void unregisterZones();

    // --- External dependency setters ---
    void setCoordBridge(oap::aa::EvdevCoordBridge* bridge);
    void setActionRegistry(ActionRegistry* registry);
    void setAudioService(QObject* svc);
    void setDisplayService(QObject* svc);

signals:
    void gestureTriggered(int controlIndex, int gesture);
    void holdProgress(int controlIndex, qreal progress);
    void edgeChanged();
    void layoutChanged();
    void popupChanged();
    void popupDrag(int controlIndex, float normalizedValue);
    void settingsPageRequested(const QString& pageId);

private:
    struct ControlState {
        bool pressed = false;
        bool longHoldFired = false;
        QElapsedTimer pressTimer;
        int activeSlot = -1;
    };

    void resetControlState(int index);
    void dispatchAction(int controlIndex, int gesture);
    void registerPopupZones(int controlIndex);
    void unregisterPopupZones();

    std::array<ControlState, 3> controls_;
    std::array<QTimer*, 3> longHoldTimers_;
    std::array<QTimer*, 3> progressTimers_;

    QString edge_ = "bottom";
    bool leftHandDrive_ = true;
    int tapMaxMs_ = 200;
    int shortHoldMaxMs_ = 600;

    // Popup state
    bool popupVisible_ = false;
    int popupControlIndex_ = -1;
    QTimer* popupDismissTimer_ = nullptr;

    // Zone registration state
    bool zonesRegistered_ = false;
    int displayWidth_ = 0;
    int displayHeight_ = 0;
    static constexpr int BAR_THICK = 56;

    // External dependencies (not owned)
    oap::aa::EvdevCoordBridge* coordBridge_ = nullptr;
    ActionRegistry* actionRegistry_ = nullptr;
    QObject* audioService_ = nullptr;
    QObject* displayService_ = nullptr;
};

} // namespace oap
