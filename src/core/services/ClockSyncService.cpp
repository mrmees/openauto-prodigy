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
    commandTimeout_ = new QTimer(this);
    commandTimeout_->setSingleShot(true);
    commandTimeout_->setInterval(5000);

    connect(commandTimeout_, &QTimer::timeout, this, [this] {
        if (!commandRunning_)
            return;
        qCWarning(lcCore) << "ClockSync: timedatectl command timed out";
        // Detach the timed-out attempt before advancing the logical queue.
        // kill() is asynchronous: its finished signal may arrive after the
        // next command has started, so that command must own a fresh process.
        QProcess* timedOutProcess = activeProcess_;
        activeProcess_ = nullptr;
        if (timedOutProcess && timedOutProcess->state() != QProcess::NotRunning)
            timedOutProcess->kill();
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

void ClockSyncService::setProcessCommandForTest(
    QString program, QStringList prefixArgs)
{
    processProgram_ = std::move(program);
    processPrefixArgs_ = std::move(prefixArgs);
}

void ClockSyncService::startTimedatectl(
    const QStringList& args, std::function<void(int)> completion)
{
    // A timed-out QProcess does not become NotRunning synchronously when it is
    // killed. Give every command an isolated process so a late finished/error
    // signal can only complete that command's serial, never its successor.
    auto* process = new QProcess(this);
    activeProcess_ = process;
    struct Attempt {
        bool completed = false;
        std::function<void(int)> completion;
    };
    auto attempt = std::make_shared<Attempt>();
    attempt->completion = std::move(completion);

    auto complete = [this, process, attempt](int exitCode) {
        if (attempt->completed)
            return;
        attempt->completed = true;
        if (activeProcess_ == process)
            activeProcess_ = nullptr;
        auto callback = std::move(attempt->completion);
        process->deleteLater();
        callback(exitCode);
    };
    connect(process, &QProcess::finished, this,
            [process, complete](int exitCode, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit)
            exitCode = -1;
        if (exitCode != 0) {
            qCWarning(lcCore) << "ClockSync: timedatectl failed:"
                              << process->readAllStandardError();
        }
        complete(exitCode);
    });
    connect(process, &QProcess::errorOccurred, this,
            [process, complete](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        qCWarning(lcCore) << "ClockSync: timedatectl failed to start:"
                          << process->errorString();
        complete(-1);
    });

    QStringList processArgs = processPrefixArgs_;
    processArgs.append(args);
    process->start(processProgram_, processArgs);
}

void ClockSyncService::enqueueCommand(
    QStringList args, std::function<void(int)> completion)
{
    commandQueue_.enqueue(
        {std::move(args), std::move(completion), CommandKind::General, {}});
    startNextCommand();
}

void ClockSyncService::enqueueTimezoneCommand(const QString& ianaId)
{
    const bool timezoneRunning = commandRunning_
        && !commandQueue_.isEmpty()
        && commandQueue_.head().kind == CommandKind::Timezone;
    int queuedTimezoneIndex = -1;
    const int firstQueuedIndex = commandRunning_ ? 1 : 0;
    for (int i = firstQueuedIndex; i < commandQueue_.size(); ++i) {
        if (commandQueue_[i].kind == CommandKind::Timezone) {
            queuedTimezoneIndex = i;
            break;
        }
    }

    // The latest report supersedes any waiting timezone. If it agrees with
    // the command already running, the queued superseding command is stale.
    if (timezoneRunning && commandQueue_.head().timezone == ianaId) {
        if (queuedTimezoneIndex >= 0)
            commandQueue_.removeAt(queuedTimezoneIndex);
        return;
    }

    // With no timezone change already in flight, a report of the effective
    // system zone cancels a queued change instead of scheduling redundant IO.
    if (!timezoneRunning && ianaId.toUtf8() == systemZone_()) {
        if (queuedTimezoneIndex >= 0)
            commandQueue_.removeAt(queuedTimezoneIndex);
        return;
    }

    Command command{
        {QStringLiteral("set-timezone"), ianaId},
        [ianaId](int rc) {
            if (rc == 0)
                qCInfo(lcCore) << "ClockSync: timezone adjusted to" << ianaId;
        },
        CommandKind::Timezone,
        ianaId,
    };
    if (queuedTimezoneIndex >= 0)
        commandQueue_[queuedTimezoneIndex] = std::move(command);
    else
        commandQueue_.enqueue(std::move(command));
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
    enqueueTimezoneCommand(ianaId);
}

} // namespace oap
