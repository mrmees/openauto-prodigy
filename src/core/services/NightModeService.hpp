#pragma once

#include <QObject>
#include <memory>

namespace oap {

class ThemeService;
class YamlConfig;

namespace aa {
class NightModeProvider;
}

/// Application-lifetime owner of the configured physical day/night source.
///
/// The service keeps one validity-gated state for both the shell theme and
/// protocol consumers. Projection sessions observe this state; they never own
/// or restart the underlying time/GPIO provider.
class NightModeService : public QObject {
    Q_OBJECT

public:
    explicit NightModeService(YamlConfig* config, ThemeService* themeService,
                              QObject* parent = nullptr);
    NightModeService(std::unique_ptr<aa::NightModeProvider> provider,
                     ThemeService* themeService, QObject* parent = nullptr);
    ~NightModeService() override;

    bool isNight() const { return currentState_; }
    bool hasValidState() const { return hasValidState_; }

    void start();
    void stop();

signals:
    void nightModeChanged(bool isNight);

private:
    void connectProvider();
    void publishValidState(bool isNight);

    std::unique_ptr<aa::NightModeProvider> provider_;
    ThemeService* themeService_ = nullptr;
    bool currentState_ = false;
    bool hasValidState_ = false;
    bool started_ = false;
};

} // namespace oap
