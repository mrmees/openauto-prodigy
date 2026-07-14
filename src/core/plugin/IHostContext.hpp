#pragma once

#include <QString>

namespace oap {

class IAudioService;
class IBluetoothService;
class IEqualizerService;
class IConfigService;
class IThemeService;
class IDisplayService;
class IEventBus;
class ActionRegistry;
class INotificationService;
class IProjectionStatusProvider;
class INavigationProvider;
class IMediaStatusProvider;
class ICallStateProvider;
class OverlayService;

enum class LogLevel { Debug, Info, Warning, Error };

class IHostContext {
public:
    virtual ~IHostContext() = default;

    virtual IAudioService* audioService() = 0;
    virtual IBluetoothService* bluetoothService() = 0;
    virtual IConfigService* configService() = 0;
    virtual IThemeService* themeService() = 0;
    virtual IDisplayService* displayService() = 0;
    virtual IEventBus* eventBus() = 0;
    virtual ActionRegistry* actionRegistry() = 0;
    virtual INotificationService* notificationService() = 0;
    virtual IEqualizerService* equalizerService() = 0;

    // Provider interfaces — return nullptr if not yet registered
    virtual IProjectionStatusProvider* projectionStatusProvider() = 0;
    virtual INavigationProvider* navigationProvider() = 0;
    virtual IMediaStatusProvider* mediaStatusProvider() = 0;
    virtual ICallStateProvider* callStateProvider() = 0;

    /// Access to the overlay framework (design §4.4) — plugins register/toggle
    /// overlays through this. Returns nullptr if not yet registered.
    virtual OverlayService* overlayService() = 0;

    /// Log a message through the host's logging system.
    /// Thread-safe.
    virtual void log(LogLevel level, const QString& message) = 0;
};

} // namespace oap
