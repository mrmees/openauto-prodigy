#include <QTest>
#include <QDateTime>
#include <QTimeZone>
#include <QSignalSpy>
#include "core/services/ClockSyncService.hpp"

// ClockSyncService: extracted clock-step / timezone-step logic shared by the
// API v1 TimeReport consumer (main.cpp) and, historically, the retired legacy
// companion listener's adjustClock (both retired in the B2 teardown,
// 2026-07-14). Semantics preserved from the legacy implementation: 30 s drift
// trigger, >5 min backward steps need 3 consecutive reports agreeing on the
// same target, timedatectl via polkit.
//
// One deliberate deviation from legacy (bug fix): the set-time argument
// carries an explicit " UTC" suffix. timedatectl parses a bare timestamp as
// LOCAL time; both legacy copies formatted the UTC wall clock and would have
// stepped the clock off by the zone offset on any non-UTC Pi.

namespace {

struct ExecRecorder {
    QList<QStringList> calls;
    int nextExitCode = 0;

    oap::ClockSyncService::ExecFn fn() {
        return [this](const QStringList& args, std::function<void(int)> completion) {
            calls.append(args);
            completion(nextExitCode);
        };
    }

    int setTimeCalls() const {
        int n = 0;
        for (const auto& c : calls)
            if (c.value(0) == QLatin1String("set-time")) ++n;
        return n;
    }
};

QString utcString(qint64 ms) {
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc())
               .toString("yyyy-MM-dd hh:mm:ss") + QStringLiteral(" UTC");
}

constexpr qint64 kNow = Q_INT64_C(1752000000000);

} // namespace

class TestClockSync : public QObject {
    Q_OBJECT

private slots:
    void smallDriftDoesNotStep() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        sync.onTimeReported(kNow + 29000);   // +29 s: below 30 s trigger
        sync.onTimeReported(kNow - 29000);   // -29 s: below trigger too

        QCOMPARE(exec.calls.size(), 0);
    }

    // timedated REFUSES SetTime while NTP is enabled (even offline and
    // unsynchronized — the normal state of an RTC-less unit in the car), so
    // every step is sandwiched: set-ntp false, set-time, set-ntp true. NTP
    // is re-enabled unconditionally: the appliance's steady state is NTP-on,
    // and real NTP should win whenever the car actually has internet.
    void forwardDriftStepsWithUtcSuffixInsideNtpSandwich() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        const qint64 phone = kNow + 35000;
        sync.onTimeReported(phone);

        QCOMPARE(exec.calls.size(), 3);
        QCOMPARE(exec.calls[0],
                 (QStringList{QStringLiteral("set-ntp"), QStringLiteral("false")}));
        QCOMPARE(exec.calls[1],
                 (QStringList{QStringLiteral("set-time"), utcString(phone)}));
        QCOMPARE(exec.calls[2],
                 (QStringList{QStringLiteral("set-ntp"), QStringLiteral("true")}));
    }

    void failedSetTimeStillRestoresNtp() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest([&exec](const QStringList& args,
                                    std::function<void(int)> completion) {
            exec.calls.append(args);
            completion(args.value(0) == QLatin1String("set-time") ? 1 : 0);
        });
        sync.setClockForTest([] { return kNow; });

        sync.onTimeReported(kNow + 35000);

        QCOMPARE(exec.calls.size(), 3);
        QCOMPARE(exec.calls[2],
                 (QStringList{QStringLiteral("set-ntp"), QStringLiteral("true")}));
    }

    void backwardWithinFiveMinutesStepsImmediately() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        const qint64 phone = kNow - 60000;   // -60 s: over trigger, under 5 min
        sync.onTimeReported(phone);

        QCOMPARE(exec.calls.size(), 3);      // set-ntp false / set-time / set-ntp true
        QCOMPARE(exec.calls[1],
                 (QStringList{QStringLiteral("set-time"), utcString(phone)}));
    }

    void largeBackwardJumpNeedsThreeAgreeingReports() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        const qint64 phone = kNow - 600000;   // -10 min
        sync.onTimeReported(phone);
        QCOMPARE(exec.calls.size(), 0);
        sync.onTimeReported(phone);
        QCOMPARE(exec.calls.size(), 0);
        sync.onTimeReported(phone);           // third agreement steps
        QCOMPARE(exec.calls.size(), 3);
        QCOMPARE(exec.calls[1],
                 (QStringList{QStringLiteral("set-time"), utcString(phone)}));
    }

    void largeBackwardJumpDisagreeingTargetsNeverStep() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        sync.onTimeReported(kNow - 600000);
        sync.onTimeReported(kNow - 601000);   // different target resets count
        sync.onTimeReported(kNow - 602000);
        sync.onTimeReported(kNow - 603000);

        QCOMPARE(exec.calls.size(), 0);
    }

    void stepResetsBackwardCounter() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        const qint64 back = kNow - 600000;
        sync.onTimeReported(back);
        sync.onTimeReported(back);            // two agreements banked
        sync.onTimeReported(kNow + 35000);    // forward step resets the bank
        QCOMPARE(exec.setTimeCalls(), 1);

        sync.onTimeReported(back);
        sync.onTimeReported(back);            // only two fresh agreements
        QCOMPARE(exec.setTimeCalls(), 1);
        sync.onTimeReported(back);            // third steps
        QCOMPARE(exec.setTimeCalls(), 2);
    }

    void execFailureIsToleratedAndRetriedNextReport() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        exec.nextExitCode = 1;
        sync.setExecForTest(exec.fn());
        sync.setClockForTest([] { return kNow; });

        sync.onTimeReported(kNow + 35000);
        QCOMPARE(exec.setTimeCalls(), 1);     // attempted, failed, no crash

        exec.nextExitCode = 0;
        sync.onTimeReported(kNow + 35000);    // next report tries again
        QCOMPARE(exec.setTimeCalls(), 2);
    }

    void timezoneMatchingSystemZoneDoesNotStep() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setSystemZoneForTest([] { return QByteArray("America/Chicago"); });

        sync.onTimezoneReported(QStringLiteral("America/Chicago"));

        QCOMPARE(exec.calls.size(), 0);
    }

    void timezoneDifferingFromSystemZoneSteps() {
        oap::ClockSyncService sync;
        ExecRecorder exec;
        sync.setExecForTest(exec.fn());
        sync.setSystemZoneForTest([] { return QByteArray("America/Chicago"); });

        sync.onTimezoneReported(QStringLiteral("Pacific/Chatham"));

        QCOMPARE(exec.calls.size(), 1);
        QCOMPARE(exec.calls[0],
                 (QStringList{QStringLiteral("set-timezone"),
                              QStringLiteral("Pacific/Chatham")}));
    }

    void timezoneReportsKeepOnlyLatestPendingChange() {
        oap::ClockSyncService sync;
        QList<QStringList> calls;
        QList<std::function<void(int)>> completions;
        sync.setSystemZoneForTest([] { return QByteArray("America/Chicago"); });
        sync.setExecForTest([&](const QStringList& args,
                                std::function<void(int)> completion) {
            calls.append(args);
            completions.append(std::move(completion));
        });

        sync.onTimezoneReported(QStringLiteral("America/Denver"));
        sync.onTimezoneReported(QStringLiteral("America/New_York"));
        sync.onTimezoneReported(QStringLiteral("Pacific/Chatham"));
        QCOMPARE(calls.size(), 1);

        completions.takeFirst()(0);
        QCOMPARE(calls.size(), 2);
        QCOMPARE(calls[1],
                 (QStringList{QStringLiteral("set-timezone"),
                              QStringLiteral("Pacific/Chatham")}));
        completions.takeFirst()(0);
        QCOMPARE(calls.size(), 2);
    }

    void latestTimezoneCanCancelOrSupersedePendingChange() {
        oap::ClockSyncService sync;
        QList<QStringList> calls;
        QList<std::function<void(int)>> completions;
        sync.setSystemZoneForTest([] { return QByteArray("America/Chicago"); });
        sync.setExecForTest([&](const QStringList& args,
                                std::function<void(int)> completion) {
            calls.append(args);
            completions.append(std::move(completion));
        });

        sync.onTimezoneReported(QStringLiteral("America/Denver"));
        sync.onTimezoneReported(QStringLiteral("America/New_York"));
        sync.onTimezoneReported(QStringLiteral("America/Denver"));
        completions.takeFirst()(0);
        QCOMPARE(calls.size(), 1);  // latest agrees with the completed command

        sync.onTimezoneReported(QStringLiteral("Pacific/Chatham"));
        sync.onTimezoneReported(QStringLiteral("America/Chicago"));
        completions.takeFirst()(0);
        QCOMPARE(calls.size(), 3);
        QCOMPARE(calls[2],
                 (QStringList{QStringLiteral("set-timezone"),
                              QStringLiteral("America/Chicago")}));
        completions.takeFirst()(0);
    }

    void commandSequenceIsAsynchronousAndDuplicateTimeReportsCoalesce() {
        oap::ClockSyncService sync;
        QList<QStringList> calls;
        QList<std::function<void(int)>> completions;
        sync.setExecForTest([&](const QStringList& args,
                                std::function<void(int)> completion) {
            calls.append(args);
            completions.append(std::move(completion));
        });
        sync.setClockForTest([] { return kNow; });
        QSignalSpy adjusted(&sync, &oap::ClockSyncService::clockAdjusted);

        sync.onTimeReported(kNow + 35000);
        QCOMPARE(calls.size(), 1);  // returned while the first command is pending
        sync.onTimeReported(kNow + 45000);
        QCOMPARE(calls.size(), 1);  // no duplicate transaction queued

        auto completeNext = [&] {
            auto completion = completions.takeFirst();
            completion(0);
        };
        completeNext();
        QCOMPARE(calls.size(), 2);
        completeNext();
        QCOMPARE(calls.size(), 3);
        QCOMPARE(adjusted.count(), 0);
        completeNext();
        QCOMPARE(adjusted.count(), 1);
        QCOMPARE(adjusted.takeFirst().at(0).toLongLong(), qint64(35000));
    }

    void commandTimeoutAdvancesThroughNtpRestoreAndAllowsRetry() {
        oap::ClockSyncService sync;
        QList<QStringList> calls;
        sync.setCommandTimeoutForTest(20);
        sync.setClockForTest([] { return kNow; });
        sync.setExecForTest([&](const QStringList& args,
                                std::function<void(int)>) {
            calls.append(args);  // deliberately never completes
        });
        QSignalSpy adjusted(&sync, &oap::ClockSyncService::clockAdjusted);

        sync.onTimeReported(kNow + 35000);
        QTRY_COMPARE_WITH_TIMEOUT(calls.size(), 3, 250);
        QTest::qWait(30);  // allow final restore timeout to close transaction
        QCOMPARE(adjusted.count(), 0);

        sync.setExecForTest([&](const QStringList& args,
                                std::function<void(int)> completion) {
            calls.append(args);
            completion(0);
        });
        sync.onTimeReported(kNow + 35000);
        QCOMPARE(calls.size(), 6);
        QCOMPARE(adjusted.count(), 1);
    }

    void realProcessTimeoutCannotConsumeSuccessorCompletion() {
        oap::ClockSyncService sync;
        sync.setCommandTimeoutForTest(50);
        sync.setClockForTest([] { return kNow; });
        // Hang only the first NTP-off command. Its kill()/finished delivery
        // races the next two immediate-success children, exercising the real
        // QProcess lifecycle rather than the injected completion seam.
        sync.setProcessCommandForTest(
            QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"),
             QStringLiteral("if [ \"$1\" = set-ntp ] && [ \"$2\" = false ]; "
                            "then exec sleep 10; fi; exit 0"),
             QStringLiteral("clock-sync-test")});
        QSignalSpy adjusted(&sync, &oap::ClockSyncService::clockAdjusted);

        sync.onTimeReported(kNow + 35000);

        QTRY_COMPARE_WITH_TIMEOUT(adjusted.count(), 1, 2000);
        QCOMPARE(adjusted.takeFirst().at(0).toLongLong(), qint64(35000));
    }
};

QTEST_MAIN(TestClockSync)
#include "test_clock_sync.moc"
