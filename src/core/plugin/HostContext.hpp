#pragma once

#include "IHostContext.hpp"

namespace oap {

class HostContext : public IHostContext {
public:
    void setAudioService(IAudioService* svc) { audio_ = svc; }
    void setBluetoothService(IBluetoothService* svc) { bt_ = svc; }
    void setConfigService(IConfigService* svc) { config_ = svc; }
    void setThemeService(IThemeService* svc) { theme_ = svc; }
    void setDisplayService(IDisplayService* svc) { display_ = svc; }
    void setEventBus(IEventBus* bus) { eventBus_ = bus; }
    void setActionRegistry(ActionRegistry* reg) { actions_ = reg; }

    IAudioService* audioService() override { return audio_; }
    IBluetoothService* bluetoothService() override { return bt_; }
    IConfigService* configService() override { return config_; }
    IThemeService* themeService() override { return theme_; }
    IDisplayService* displayService() override { return display_; }
    IEventBus* eventBus() override { return eventBus_; }
    ActionRegistry* actionRegistry() override { return actions_; }

    void log(LogLevel level, const QString& message) override;

private:
    IAudioService* audio_ = nullptr;
    IBluetoothService* bt_ = nullptr;
    IConfigService* config_ = nullptr;
    IThemeService* theme_ = nullptr;
    IDisplayService* display_ = nullptr;
    IEventBus* eventBus_ = nullptr;
    ActionRegistry* actions_ = nullptr;
};

} // namespace oap
