#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace oap {

// Steps the RTC-less head unit's clock and timezone from the phone's
// wall-clock report (API v1 TimeReport). Single tested home for the logic
// that previously lived twice: CompanionListenerService::adjustClock (legacy
// 9876 path, retired at B2) and a static mirror in main.cpp.
//
// Semantics: 30 s drift trigger; steps backward larger than 5 minutes need
// 3 consecutive reports agreeing on the same target before they apply;
// timezone steps are skipped when the reported zone already matches the
// system zone. All steps go through timedatectl (polkit-authorized, see
// config/companion-polkit.rules).
//
// The set-time argument carries an explicit " UTC" suffix: timedatectl
// parses a bare timestamp as LOCAL time, so the bare UTC wall-clock string
// both legacy copies produced would step the clock off by the zone offset
// on any non-UTC system.
class ClockSyncService : public QObject {
    Q_OBJECT
public:
    // Runs `timedatectl <args>`, returns its exit code.
    using ExecFn = std::function<int(const QStringList& args)>;
    using ClockFn = std::function<qint64()>;
    using ZoneFn = std::function<QByteArray()>;

    explicit ClockSyncService(QObject* parent = nullptr);

    void setExecForTest(ExecFn fn) { exec_ = std::move(fn); }
    void setClockForTest(ClockFn fn) { now_ = std::move(fn); }
    void setSystemZoneForTest(ZoneFn fn) { systemZone_ = std::move(fn); }

public slots:
    void onTimeReported(qint64 phoneTimeMs);
    void onTimezoneReported(const QString& ianaId);

private:
    ExecFn exec_;
    ClockFn now_;
    ZoneFn systemZone_;
    int backwardJumpCount_ = 0;
    qint64 lastBackwardTarget_ = 0;
};

} // namespace oap
