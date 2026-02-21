#pragma once

#include <QString>

namespace oap {

class IAudioService;
class IBluetoothService;
class IConfigService;
class IThemeService;
class IDisplayService;
class IEventBus;
class ActionRegistry;
class INotificationService;

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

    /// Log a message through the host's logging system.
    /// Thread-safe.
    virtual void log(LogLevel level, const QString& message) = 0;
};

} // namespace oap
