#pragma once

#include <QObject>
#include <QString>
#include <QStack>
#include "ApplicationTypes.hpp"

namespace oap {

class ApplicationController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int currentApplication READ currentApplication NOTIFY currentApplicationChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentTitleChanged)

public:
    explicit ApplicationController(QObject *parent = nullptr);

    int currentApplication() const { return currentApp_; }
    QString currentTitle() const { return currentTitle_; }

    /// @deprecated Use PluginModel::setActivePlugin() for plugin navigation.
    /// Kept for built-in screens (settings) that aren't yet plugins.
    Q_INVOKABLE void navigateTo(int appType);
    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void setTitle(const QString &title);
    Q_INVOKABLE void quit();
    Q_INVOKABLE void minimize();

signals:
    void currentApplicationChanged();
    void currentTitleChanged();

private:
    int currentApp_ = ApplicationTypes::Launcher;
    QString currentTitle_ = QStringLiteral("OpenAuto Prodigy");
    QStack<int> navigationStack_;
};

} // namespace oap
