#include "ui/ApplicationController.hpp"
#include <QGuiApplication>
#include <QProcess>
#include <QTimer>
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

void ApplicationController::exitToDesktop()
{
    qInfo() << "[App] Exit to desktop requested";

    const bool isKiosk = qEnvironmentVariableIsSet("OPENAUTO_KIOSK");

    if (isKiosk) {
        // Kiosk mode: tell LightDM to switch to the normal desktop session.
        // dm-tool switch-to-user does an atomic session switch: LightDM starts
        // rpd-labwc on a new VT and switches the display to it.
        bool launched = QProcess::startDetached("dm-tool", {"switch-to-user",
            qEnvironmentVariable("USER", "matt"), "rpd-labwc"});

        if (!launched) {
            // dm-tool failed — do NOT quit or we'll strand on a black screen
            // (Pitfall 9). User can still SSH in or reboot.
            qWarning() << "[App] dm-tool launch failed — staying in kiosk mode";
            return;
        }

        // Delay quit to give dm-tool time to register the switch with LightDM.
        // Exit code 0 = clean exit, so systemd Restart=on-failure won't restart.
        QTimer::singleShot(500, []() {
            QCoreApplication::quit();
        });
    } else {
        // Desktop mode: just quit — the desktop is already underneath.
        QCoreApplication::quit();
    }
}

} // namespace oap
