#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <functional>

class QProcess;
class QTimer;

namespace oap {

// Steps the RTC-less head unit's clock and timezone from the phone's
// wall-clock report (API v1 TimeReport). Single tested home for the logic
// that previously lived twice (the legacy companion listener and a static
// mirror in main.cpp; both retired in the B2 teardown, 2026-07-14).
//
// Semantics: 30 s drift trigger; steps backward larger than 5 minutes need
// 3 consecutive reports agreeing on the same target before they apply;
// timezone steps are skipped when the reported zone already matches the
// system zone. All steps go through timedatectl (polkit-authorized, see
// config/clock-sync-polkit.rules).
//
// The set-time argument carries an explicit " UTC" suffix: timedatectl
// parses a bare timestamp as LOCAL time, so the bare UTC wall-clock string
// both legacy copies produced would step the clock off by the zone offset
// on any non-UTC system.
class ClockSyncService : public QObject {
    Q_OBJECT
public:
    // Starts `timedatectl <args>` and invokes completion asynchronously.
    using ExecFn = std::function<void(
        const QStringList& args, std::function<void(int)> completion)>;
    using ClockFn = std::function<qint64()>;
    using ZoneFn = std::function<QByteArray()>;

    explicit ClockSyncService(QObject* parent = nullptr);

    void setExecForTest(ExecFn fn) { exec_ = std::move(fn); }
    void setClockForTest(ClockFn fn) { now_ = std::move(fn); }
    void setSystemZoneForTest(ZoneFn fn) { systemZone_ = std::move(fn); }
    void setCommandTimeoutForTest(int ms);

public slots:
    void onTimeReported(qint64 phoneTimeMs);
    void onTimezoneReported(const QString& ianaId);

signals:
    void clockAdjusted(qint64 deltaMs);

private:
    struct Command {
        QStringList args;
        std::function<void(int)> completion;
    };

    void enqueueCommand(QStringList args, std::function<void(int)> completion = {});
    void startNextCommand();
    void finishCurrentCommand(quint64 serial, int exitCode);
    void startTimedatectl(const QStringList& args, std::function<void(int)> completion);

    ExecFn exec_;
    ClockFn now_;
    ZoneFn systemZone_;
    int backwardJumpCount_ = 0;
    qint64 lastBackwardTarget_ = 0;
    QQueue<Command> commandQueue_;
    bool commandRunning_ = false;
    bool timeSyncPending_ = false;
    quint64 commandSerial_ = 0;
    QProcess* process_ = nullptr;
    QTimer* commandTimeout_ = nullptr;
    std::function<void(int)> processCompletion_;
};

} // namespace oap
