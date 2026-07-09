#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "plugins/media_player/FolderModel.hpp"

using oap::plugins::FolderModel;

class TestFolderModel : public QObject {
    Q_OBJECT
private slots:
    void init();          // fresh fixture tree per test
    void testTopLevelShowsRoots();
    void testEnterListsDirsFirstThenAudioSorted();
    void testNonAudioFilesExcluded();
    void testUpNavigation();
    void testAudioFilesInCurrentDir();
    void testEmptyAndMissingDirSafe();

private:
    QString rowName(const FolderModel& m, int row) const {
        return m.data(m.index(row, 0), FolderModel::NameRole).toString();
    }
    bool rowIsDir(const FolderModel& m, int row) const {
        return m.data(m.index(row, 0), FolderModel::IsDirRole).toBool();
    }
    QString rowPath(const FolderModel& m, int row) const {
        return m.data(m.index(row, 0), FolderModel::PathRole).toString();
    }
    void makeFile(const QString& path) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    QScopedPointer<QTemporaryDir> tmp_;
    QString root_;
};

void TestFolderModel::init() {
    tmp_.reset(new QTemporaryDir);
    QVERIFY(tmp_->isValid());
    root_ = tmp_->path();
    QDir(root_).mkpath("Zeppelin/IV");
    QDir(root_).mkpath("Apple");
    makeFile(root_ + "/Zeppelin/02 Rock and Roll.mp3");
    makeFile(root_ + "/Zeppelin/01 Black Dog.flac");
    makeFile(root_ + "/Zeppelin/cover.jpg");
    makeFile(root_ + "/Zeppelin/notes.txt");
    makeFile(root_ + "/Zeppelin/IV/04 Stairway.opus");
}

void TestFolderModel::testTopLevelShowsRoots() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_},
                {QStringLiteral("USB1"), root_ + "/Zeppelin"}});
    QVERIFY(m.atTopLevel());
    QCOMPARE(m.rowCount(), 2);
    QCOMPARE(rowName(m, 0), QString("Music"));
    QCOMPARE(rowName(m, 1), QString("USB1"));
    QVERIFY(rowIsDir(m, 0));
}

void TestFolderModel::testEnterListsDirsFirstThenAudioSorted() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    QVERIFY(!m.atTopLevel());
    // dirs first (IV), then audio sorted: 01 Black Dog.flac, 02 Rock and Roll.mp3
    QCOMPARE(m.rowCount(), 3);
    QCOMPARE(rowName(m, 0), QString("IV"));
    QVERIFY(rowIsDir(m, 0));
    QCOMPARE(rowName(m, 1), QString("01 Black Dog.flac"));
    QVERIFY(!rowIsDir(m, 1));
    QCOMPARE(rowName(m, 2), QString("02 Rock and Roll.mp3"));
}

void TestFolderModel::testNonAudioFilesExcluded() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    for (int i = 0; i < m.rowCount(); ++i) {
        QVERIFY(!rowName(m, i).endsWith(".jpg"));
        QVERIFY(!rowName(m, i).endsWith(".txt"));
    }
}

void TestFolderModel::testUpNavigation() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    m.enter(root_ + "/Zeppelin/IV");
    QVERIFY(m.breadcrumb().contains("IV"));
    QVERIFY(m.up());                       // -> Zeppelin
    QVERIFY(m.up());                       // -> root_ (the "Music" root dir)
    QVERIFY(m.up());                       // -> top level (root list)
    QVERIFY(m.atTopLevel());
    QVERIFY(!m.up());                      // already at top
}

void TestFolderModel::testAudioFilesInCurrentDir() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    const QStringList files = m.audioFilesInCurrentDir();
    QCOMPARE(files.size(), 2);
    QVERIFY(files.at(0).endsWith("01 Black Dog.flac"));
    QVERIFY(files.at(1).endsWith("02 Rock and Roll.mp3"));
}

void TestFolderModel::testEmptyAndMissingDirSafe() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_ + "/Apple"}});
    m.enter(root_ + "/Apple");
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.audioFilesInCurrentDir(), QStringList());
    m.enter(root_ + "/DoesNotExist");      // must not crash; stays put or empties
    QVERIFY(m.rowCount() >= 0);
}

QTEST_GUILESS_MAIN(TestFolderModel)
#include "test_folder_model.moc"
