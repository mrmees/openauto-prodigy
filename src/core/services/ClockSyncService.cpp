#include "ClockSyncService.hpp"
#include "../Logging.hpp"

#include <QDateTime>
#include <QProcess>
#include <QTimeZone>

namespace oap {

namespace {

int runTimedatectl(const QStringList& args)
{
    QProcess proc;
    proc.start(QStringLiteral("timedatectl"), args);
    proc.waitForFinished(5000);
    if (proc.exitCode() != 0) {
        qCWarning(lcCore) << "ClockSync: timedatectl" << args.value(0)
                          << "failed:" << proc.readAllStandardError();
    }
    return proc.exitCode();
}

} // namespace

ClockSyncService::ClockSyncService(QObject* parent)
    : QObject(parent)
    , exec_(runTimedatectl)
    , now_([] { return QDateTime::currentMSecsSinceEpoch(); })
    , systemZone_([] { return QTimeZone::systemTimeZoneId(); })
{
}

void ClockSyncService::onTimeReported(qint64 phoneTimeMs)
{
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

    if (exec_({QStringLiteral("set-time"), timeStr}) == 0) {
        qCInfo(lcCore) << "ClockSync: clock adjusted by" << deltaMs << "ms"
                       << "(" << piTimeMs << "->" << phoneTimeMs << ")";
    }
}

void ClockSyncService::onTimezoneReported(const QString& ianaId)
{
    if (ianaId.toUtf8() == systemZone_()) return;

    if (exec_({QStringLiteral("set-timezone"), ianaId}) == 0) {
        qCInfo(lcCore) << "ClockSync: timezone adjusted to" << ianaId;
    }
}

} // namespace oap
