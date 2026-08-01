#include <QTest>
#include <QSignalSpy>

#include "core/aa/NavigationLaneModel.hpp"

class TestNavigationLaneModel : public QObject {
    Q_OBJECT

private slots:
    void testRowsExposeOrderedDirectionMaps() {
        oap::aa::NavigationLaneModel model;
        const oap::aa::LanePresentationList lanes{
            {{QStringLiteral("straight"), false},
             {QStringLiteral("normal_right"), true}},
            {{QStringLiteral("u_turn_left"), true}},
        };

        model.replaceLanes(lanes);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.roleNames().value(
                     oap::aa::NavigationLaneModel::DirectionsRole),
                 QByteArray("directions"));

        const QVariantList firstDirections = model.data(
            model.index(0), oap::aa::NavigationLaneModel::DirectionsRole)
                                                   .toList();
        QCOMPARE(firstDirections.size(), 2);
        const QVariantMap first = firstDirections[0].toMap();
        QCOMPARE(first.size(), 2);
        QCOMPARE(first.value(QStringLiteral("shape")).toString(),
                 QStringLiteral("straight"));
        QCOMPARE(first.value(QStringLiteral("recommended")).toBool(), false);
        const QVariantMap second = firstDirections[1].toMap();
        QCOMPARE(second.size(), 2);
        QCOMPARE(second.value(QStringLiteral("shape")).toString(),
                 QStringLiteral("normal_right"));
        QCOMPARE(second.value(QStringLiteral("recommended")).toBool(), true);

        const QVariantList secondLane = model.data(
            model.index(1), oap::aa::NavigationLaneModel::DirectionsRole)
                                            .toList();
        QCOMPARE(secondLane.size(), 1);
        QCOMPARE(secondLane[0].toMap().value(QStringLiteral("shape")).toString(),
                 QStringLiteral("u_turn_left"));
    }

    void testReplaceLanesResetsSnapshot() {
        oap::aa::NavigationLaneModel model;
        model.replaceLanes({
            {{QStringLiteral("straight"), false}},
            {{QStringLiteral("normal_left"), true}},
        });

        model.replaceLanes({
            {{QStringLiteral("sharp_right"), true}},
        });

        QCOMPARE(model.rowCount(), 1);
        const QVariantList directions = model.data(
            model.index(0), oap::aa::NavigationLaneModel::DirectionsRole)
                                            .toList();
        QCOMPARE(directions.size(), 1);
        QCOMPARE(directions[0].toMap().value(QStringLiteral("shape")).toString(),
                 QStringLiteral("sharp_right"));
    }

    void testClearRemovesAllRows() {
        oap::aa::NavigationLaneModel model;
        model.replaceLanes({{{QStringLiteral("straight"), true}}});

        model.clear();

        QCOMPARE(model.rowCount(), 0);
    }

    void testIdenticalSnapshotDoesNotResetModel() {
        oap::aa::NavigationLaneModel model;
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        const oap::aa::LanePresentationList lanes{
            {{QStringLiteral("straight"), false},
             {QStringLiteral("normal_right"), true}},
            {{QStringLiteral("u_turn_left"), true}},
        };

        model.replaceLanes(lanes);
        QCOMPARE(resetSpy.count(), 1);

        model.replaceLanes(lanes);
        QCOMPARE(resetSpy.count(), 1);
    }

    void testRepeatedClearWhileEmptyDoesNotResetModel() {
        oap::aa::NavigationLaneModel model;
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        model.clear();
        model.clear();

        QCOMPARE(resetSpy.count(), 0);
    }

    void testChangedSnapshotStillResetsModelOnce() {
        oap::aa::NavigationLaneModel model;
        model.replaceLanes({{{QStringLiteral("straight"), false}}});
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        model.replaceLanes({{{QStringLiteral("sharp_right"), true}}});

        QCOMPARE(resetSpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestNavigationLaneModel)
#include "test_navigation_lane_model.moc"
