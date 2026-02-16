#pragma once

#include <QObject>
#include <QColor>
#include <memory>
#include "core/Configuration.hpp"

namespace oap {

class ThemeController : public QObject {
    Q_OBJECT

    Q_PROPERTY(ThemeMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor controlBackgroundColor READ controlBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor controlForegroundColor READ controlForegroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor normalFontColor READ normalFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor specialFontColor READ specialFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor descriptionFontColor READ descriptionFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor barBackgroundColor READ barBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor controlBoxBackgroundColor READ controlBoxBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor gaugeIndicatorColor READ gaugeIndicatorColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor iconColor READ iconColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor sideWidgetBackgroundColor READ sideWidgetBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor barShadowColor READ barShadowColor NOTIFY colorsChanged)

public:
    explicit ThemeController(std::shared_ptr<Configuration> config, QObject *parent = nullptr);

    ThemeMode mode() const { return mode_; }
    void setMode(ThemeMode mode);

    QColor backgroundColor() const { return config_->backgroundColor(mode_); }
    QColor highlightColor() const { return config_->highlightColor(mode_); }
    QColor controlBackgroundColor() const { return config_->controlBackgroundColor(mode_); }
    QColor controlForegroundColor() const { return config_->controlForegroundColor(mode_); }
    QColor normalFontColor() const { return config_->normalFontColor(mode_); }
    QColor specialFontColor() const { return config_->specialFontColor(mode_); }
    QColor descriptionFontColor() const { return config_->descriptionFontColor(mode_); }
    QColor barBackgroundColor() const { return config_->barBackgroundColor(mode_); }
    QColor controlBoxBackgroundColor() const { return config_->controlBoxBackgroundColor(mode_); }
    QColor gaugeIndicatorColor() const { return config_->gaugeIndicatorColor(mode_); }
    QColor iconColor() const { return config_->iconColor(mode_); }
    QColor sideWidgetBackgroundColor() const { return config_->sideWidgetBackgroundColor(mode_); }
    QColor barShadowColor() const { return config_->barShadowColor(mode_); }

    Q_INVOKABLE void toggleMode();

signals:
    void modeChanged();
    void colorsChanged();

private:
    std::shared_ptr<Configuration> config_;
    ThemeMode mode_ = ThemeMode::Day;
};

} // namespace oap
