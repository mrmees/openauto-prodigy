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

void ApplicationController::requestBack()
{
    backHandled_ = false;
    emit backRequested();
    if (!backHandled_)
        navigateBack();
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

void ApplicationController::restart()
{
    // Exit with non-zero code so systemd's Restart=on-failure restarts us.
    // The old detached-shell approach failed because systemd kills the
    // cgroup on service exit, taking the relaunch shell with it.
    qInfo() << "[App] Restart requested — exiting with code 1 for systemd restart";
    QCoreApplication::exit(1);
}

void ApplicationController::minimize()
{
    auto windows = QGuiApplication::topLevelWindows();
    if (!windows.isEmpty())
        windows.first()->showMinimized();
}

} // namespace oap
