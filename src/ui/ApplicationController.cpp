#include "ui/ApplicationController.hpp"
#include <QGuiApplication>
#include <QProcess>
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

void ApplicationController::restart()
{
    // Wait for this process to fully exit (port release, etc.) before
    // starting the new instance. Uses the PID to poll for death.
    qint64 pid = QCoreApplication::applicationPid();
    QString appPath = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();

    // Build the relaunch command with args
    QString relaunch = appPath;
    for (int i = 1; i < args.size(); ++i)
        relaunch += " " + args[i];

    QString cmd = QString(
        "while kill -0 %1 2>/dev/null; do sleep 0.1; done; exec %2"
    ).arg(pid).arg(relaunch);

    QProcess::startDetached("/bin/sh", {"-c", cmd});
    QGuiApplication::quit();
}

void ApplicationController::minimize()
{
    auto windows = QGuiApplication::topLevelWindows();
    if (!windows.isEmpty())
        windows.first()->showMinimized();
}

} // namespace oap
