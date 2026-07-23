#include "ClockSyncService.hpp"
#include "../Logging.hpp"

#include <QDateTime>
#include <QProcess>
#include <QTimer>
#include <QTimeZone>
#include <memory>

namespace oap {

ClockSyncService::ClockSyncService(QObject* parent)
    : QObject(parent)
    , now_([] { return QDateTime::currentMSecsSinceEpoch(); })
    , systemZone_([] { return QTimeZone::systemTimeZoneId(); })
{
    process_ = new QProcess(this);
    commandTimeout_ = new QTimer(this);
    commandTimeout_->setSingleShot(true);
    commandTimeout_->setInterval(5000);

    connect(process_, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
        auto completion = std::move(processCompletion_);
        processCompletion_ = {};
        if (!completion)
            return;
        if (status != QProcess::NormalExit)
            exitCode = -1;
        if (exitCode != 0) {
            qCWarning(lcCore) << "ClockSync: timedatectl failed:"
                              << process_->readAllStandardError();
        }
        completion(exitCode);
    });
    connect(process_, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !processCompletion_)
            return;
        auto completion = std::move(processCompletion_);
        processCompletion_ = {};
        qCWarning(lcCore) << "ClockSync: timedatectl failed to start:"
                          << process_->errorString();
        completion(-1);
    });
    connect(commandTimeout_, &QTimer::timeout, this, [this] {
        if (!commandRunning_)
            return;
        qCWarning(lcCore) << "ClockSync: timedatectl command timed out";
        processCompletion_ = {};
        if (process_->state() != QProcess::NotRunning)
            process_->kill();
        finishCurrentCommand(commandSerial_, -1);
    });

    exec_ = [this](const QStringList& args, std::function<void(int)> completion) {
        startTimedatectl(args, std::move(completion));
    };
}

void ClockSyncService::setCommandTimeoutForTest(int ms)
{
    commandTimeout_->setInterval(ms);
}

void ClockSyncService::startTimedatectl(
    const QStringList& args, std::function<void(int)> completion)
{
    processCompletion_ = std::move(completion);
    process_->start(QStringLiteral("timedatectl"), args);
}

void ClockSyncService::enqueueCommand(
    QStringList args, std::function<void(int)> completion)
{
    commandQueue_.enqueue({std::move(args), std::move(completion)});
    startNextCommand();
}

void ClockSyncService::startNextCommand()
{
    if (commandRunning_ || commandQueue_.isEmpty())
        return;
    commandRunning_ = true;
    const quint64 serial = ++commandSerial_;
    commandTimeout_->start();
    const QStringList args = commandQueue_.head().args;
    exec_(args, [this, serial](int exitCode) {
        finishCurrentCommand(serial, exitCode);
    });
}

void ClockSyncService::finishCurrentCommand(quint64 serial, int exitCode)
{
    if (!commandRunning_ || serial != commandSerial_)
        return;
    commandTimeout_->stop();
    Command command = commandQueue_.dequeue();
    commandRunning_ = false;
    if (command.completion)
        command.completion(exitCode);
    startNextCommand();
}

void ClockSyncService::onTimeReported(qint64 phoneTimeMs)
{
    if (timeSyncPending_)
        return;
    const qint64 piTimeMs = now_();
    const qint64 deltaMs = phoneTimeMs - piTimeMs;

    // Only adjust if drift exceeds 30 seconds
    if (qAbs(deltaMs) < 30000) return;

    // Backward jump protection: reject >5min backward unless 3 consecutive agree
    if (deltaMs < -300000) {
        if (phoneTimeMs == lastBackwardTarget_) {
            backwardJumpCount_++;
        } else {
            backwardJumpCount_ = 1;
            lastBackwardTarget_ = phoneTimeMs;
        }
        if (backwardJumpCount_ < 3) return;
    }
    backwardJumpCount_ = 0;
    lastBackwardTarget_ = 0;

    const QDateTime newTime =
        QDateTime::fromMSecsSinceEpoch(phoneTimeMs, QTimeZone::utc());
    const QString timeStr =
        newTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")) + QStringLiteral(" UTC");

    // timedated REFUSES SetTime while NTP is enabled — even offline and
    // unsynchronized, the normal state of an RTC-less unit in the car. Step
    // inside a set-ntp sandwich and re-enable unconditionally afterwards
    // (success or not): NTP-on is the appliance's steady state, and real NTP
    // should win whenever the car actually has internet (e.g. over the
    // companion's SOCKS5 route).
    timeSyncPending_ = true;
    auto setTimeResult = std::make_shared<int>(-1);
    enqueueCommand({QStringLiteral("set-ntp"), QStringLiteral("false")});
    enqueueCommand({QStringLiteral("set-time"), timeStr},
                   [setTimeResult](int rc) { *setTimeResult = rc; });
    enqueueCommand({QStringLiteral("set-ntp"), QStringLiteral("true")},
                   [this, setTimeResult, deltaMs, piTimeMs, phoneTimeMs](int) {
        timeSyncPending_ = false;
        if (*setTimeResult == 0) {
            qCInfo(lcCore) << "ClockSync: clock adjusted by" << deltaMs << "ms"
                           << "(" << piTimeMs << "->" << phoneTimeMs << ")";
            emit clockAdjusted(deltaMs);
        }
    });
}

void ClockSyncService::onTimezoneReported(const QString& ianaId)
{
    if (ianaId.toUtf8() == systemZone_()) return;

    enqueueCommand({QStringLiteral("set-timezone"), ianaId},
                   [ianaId](int rc) {
        if (rc == 0)
            qCInfo(lcCore) << "ClockSync: timezone adjusted to" << ianaId;
    });
}

} // namespace oap
