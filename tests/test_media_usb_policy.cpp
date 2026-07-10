#include <QtTest>
#include "plugins/media_player/PlayQueue.hpp"
#include "plugins/media_player/UsbMediaWatcher.hpp"  // parseFirstMountPoint
// (Step 3 creates a minimal UsbMediaWatcher.hpp with the helper defined
// INLINE so this test compiles and links before the D-Bus glue exists.)

using oap::plugins::PlayQueue;

class TestMediaUsbPolicy : public QObject {
    Q_OBJECT
private slots:
    void purgeKeepsCurrentWhenItSurvives() {
        PlayQueue q;
        q.setTracks({"/home/m/a.mp3", "/media/usb/b.mp3", "/home/m/c.mp3"}, 0);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 1);
        QCOMPARE(q.count(), 2);
        QCOMPARE(q.currentTrack(), QStringLiteral("/home/m/a.mp3"));
    }
    void purgeAdvancesWhenCurrentDies() {
        PlayQueue q;
        q.setTracks({"/media/usb/a.mp3", "/home/m/b.mp3", "/media/usb/c.mp3"}, 0);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 2);
        QCOMPARE(q.count(), 1);
        QCOMPARE(q.currentTrack(), QStringLiteral("/home/m/b.mp3"));
    }
    void purgeCanEmptyTheQueue() {
        PlayQueue q;
        q.setTracks({"/media/usb/a.mp3", "/media/usb/b.mp3"}, 1);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 2);
        QCOMPARE(q.count(), 0);
        QVERIFY(q.currentTrack().isEmpty());
    }
    void prefixIsPathBoundary() {
        PlayQueue q;
        q.setTracks({"/media/usb2/x.mp3", "/media/usb/y.mp3"}, 0);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 1);   // usb2 survives
        QCOMPARE(q.count(), 1);
    }
    void shuffledPurgeKeepsCurrentAndFullTraversal() {
        // Codex re-run P2: assert the ENTIRE remaining traversal, not just
        // the current path — a purge that reshuffles must fail here.
        const QStringList tracks{"/h/a.mp3", "/media/usb/x.mp3", "/h/b.mp3", "/h/c.mp3"};
        // Twin: record the full seeded traversal from the same start.
        PlayQueue twin;
        twin.setShuffleSeed(42);
        twin.setTracks(tracks, 0);
        twin.setShuffle(true);
        QStringList expected{twin.currentTrack()};
        while (twin.advance(true)) expected << twin.currentTrack();
        // Expected post-purge traversal: the same sequence minus USB tracks.
        QStringList expectedSurvivors;
        for (const QString& t : expected)
            if (!t.startsWith(QLatin1String("/media/usb/"))) expectedSurvivors << t;

        PlayQueue q;
        q.setShuffleSeed(42);
        q.setTracks(tracks, 0);
        q.setShuffle(true);
        q.removeTracksUnder("/media/usb/");
        QStringList actual{q.currentTrack()};
        while (q.advance(true)) actual << q.currentTrack();
        QCOMPARE(actual, expectedSurvivors);
    }
    void wrapToFirstSurvivorWhenNoneFollow() {
        // Contract: current removed and no survivor FOLLOWS it in traversal
        // order -> wrap to the FIRST survivor (queue stays loaded; the
        // plugin decides whether anything plays).
        PlayQueue q;
        q.setTracks({"/h/b.mp3", "/media/usb/c.mp3"}, 1);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 1);
        QCOMPARE(q.currentTrack(), QStringLiteral("/h/b.mp3"));
    }
    void shuffledPurgeOfCurrentPicksTraversalSuccessor() {
        // Twin queue with the same seed tells us the traversal successor;
        // purging the current track must land exactly there.
        const QStringList tracks{"/u/a.mp3", "/h/b.mp3", "/u/c.mp3", "/h/d.mp3"};
        PlayQueue twin;
        twin.setShuffleSeed(7);
        twin.setTracks(tracks, 0);
        twin.setShuffle(true);
        QString successor;
        do { twin.advance(true); successor = twin.currentTrack(); }
        while (successor.startsWith(QLatin1String("/u/")));

        PlayQueue q;
        q.setShuffleSeed(7);
        q.setTracks(tracks, 0);
        q.setShuffle(true);
        q.removeTracksUnder("/u/");     // removes the current "/u/a.mp3" too
        QCOMPARE(q.currentTrack(), successor);
    }
    void parsesMountPointsAay() {
        // UDisks MountPoints is 'aay' — NUL-terminated byte arrays.
        QCOMPARE(oap::plugins::parseFirstMountPoint({QByteArray("/media/usb\0", 11)}),
                 QStringLiteral("/media/usb"));
        QCOMPARE(oap::plugins::parseFirstMountPoint({}), QString());
    }

    // --- usbCandidateDecision(): pure eligibility + Block-hint policy ---
    // Mirrors the persistent-cache design (usb-cache-design-sol.md items 4-6):
    // Drive props settle LATER than the hot-plug event, so the verdict must be
    // recomputable and must defer (not reject) until identity is known.

    void deferWhileDriveUnknownThenRegisters() {
        using D = oap::plugins::UsbCandidateDecision;
        // 3 ms after hot-plug the Drive is unknown -> Defer, never a hard reject.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     /*driveKnown*/ false, /*removable*/ false, /*bus*/ QString(),
                     /*hintIgnore*/ false, /*hintSystem*/ false, /*hintAuto*/ true,
                     /*mounted*/ false),
                 D::Defer);
        // Seconds later Removable settles true; still unmounted + HintAuto -> mount.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QString(), false, false, true, false),
                 D::AutoMount);
        // If it settled true AND was already mounted, register it straight away.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QString(), false, false, false, true),
                 D::RegisterMounted);
    }

    void usbFallbackEligibility() {
        using D = oap::plugins::UsbCandidateDecision;
        // Removable=false but ConnectionBus="usb" -> eligible (USB thumb drives
        // carry non-removable media inside a removable device).
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, false, QStringLiteral("usb"), false, false, true, false),
                 D::AutoMount);
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, false, QStringLiteral("usb"), false, false, false, true),
                 D::RegisterMounted);
        // Drive known, not removable, not usb -> stays dormant (Defer), never a
        // reject: a later PropertiesChanged could still flip Removable true.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, false, QStringLiteral("sata"), false, false, true, false),
                 D::Defer);
    }

    void hintSystemAndIgnoreAlwaysReject() {
        using D = oap::plugins::UsbCandidateDecision;
        // HintSystem rejects even a removable, mounted, USB device.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QStringLiteral("usb"), false, true, true, true),
                 D::Reject);
        // HintIgnore likewise.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QStringLiteral("usb"), true, false, true, false),
                 D::Reject);
        // And the hard reject applies even before the Drive is known.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     false, false, QString(), false, true, false, false),
                 D::Reject);
    }

    void hintAutoGatesUnmountedMount() {
        using D = oap::plugins::UsbCandidateDecision;
        // Eligible + unmounted + HintAuto -> auto-mount.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QString(), false, false, true, false),
                 D::AutoMount);
        // Same but HintAuto=false -> track only, do NOT mount.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QString(), false, false, false, false),
                 D::TrackOnly);
    }

    void mountedRegistersRegardlessOfHintAuto() {
        using D = oap::plugins::UsbCandidateDecision;
        // Eligible + already mounted registers even when HintAuto is false.
        QCOMPARE(oap::plugins::usbCandidateDecision(
                     true, true, QString(), false, false, false, true),
                 D::RegisterMounted);
    }
};

QTEST_APPLESS_MAIN(TestMediaUsbPolicy)
#include "test_media_usb_policy.moc"
