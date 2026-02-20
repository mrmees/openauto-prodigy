#include "ui/ApplicationController.hpp"
#include <QGuiApplication>
#include <QWindow>

namespace oap {

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent)
{
}

void ApplicationController::navigateTo(int appType)
{
    if (appType == currentApp_)
        return;

    navigationStack_.push(currentApp_);
    currentApp_ = appType;
    emit currentApplicationChanged();
}

void ApplicationController::navigateBack()
{
    if (navigationStack_.isEmpty())
        return;

    currentApp_ = navigationStack_.pop();
    emit currentApplicationChanged();
}

void ApplicationController::setTitle(const QString &title)
{
    if (currentTitle_ == title)
        return;
    currentTitle_ = title;
    emit currentTitleChanged();
}

void ApplicationController::quit()
{
    QGuiApplication::quit();
}

void ApplicationController::minimize()
{
    auto windows = QGuiApplication::topLevelWindows();
    if (!windows.isEmpty())
        windows.first()->showMinimized();
}

} // namespace oap
