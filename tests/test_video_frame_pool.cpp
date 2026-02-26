#include <QTest>
#include "core/aa/VideoFramePool.hpp"
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <vector>

class TestVideoFramePool : public QObject {
    Q_OBJECT
private slots:
    void testAcquireReturnsValidFrame();
    void testPoolTracksAllocations();
    void testPoolGrowsOnDemand();
    void testFormatChange();
    void testResetClearsCount();
};

void TestVideoFramePool::testAcquireReturnsValidFrame()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    QVideoFrame frame = pool.acquire();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.size(), QSize(1280, 720));
}

void TestVideoFramePool::testPoolTracksAllocations()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    QCOMPARE(pool.totalAllocated(), 0);

    for (int i = 0; i < 10; ++i) {
        QVideoFrame frame = pool.acquire();
        QVERIFY(frame.isValid());
    }

    QCOMPARE(pool.totalAllocated(), 10);
}

void TestVideoFramePool::testPoolGrowsOnDemand()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 2);

    // Hold multiple frames simultaneously — pool must handle it
    std::vector<QVideoFrame> held;
    for (int i = 0; i < 5; ++i) {
        held.push_back(pool.acquire());
        QVERIFY(held.back().isValid());
    }
    QCOMPARE(pool.totalAllocated(), 5);
}

void TestVideoFramePool::testFormatChange()
{
    QVideoFrameFormat fmt720({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt720, 3);
    pool.acquire();
    QCOMPARE(pool.totalAllocated(), 1);

    QVideoFrameFormat fmt1080({1920, 1080}, QVideoFrameFormat::Format_YUV420P);
    pool.reset(fmt1080);

    QVideoFrame frame = pool.acquire();
    QCOMPARE(frame.size(), QSize(1920, 1080));
    // reset() clears the allocation counter
    QCOMPARE(pool.totalAllocated(), 1);
}

void TestVideoFramePool::testResetClearsCount()
{
    QVideoFrameFormat fmt({800, 480}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    for (int i = 0; i < 5; ++i)
        pool.acquire();
    QCOMPARE(pool.totalAllocated(), 5);

    // Reset with same format still clears the counter
    pool.reset(fmt);
    QCOMPARE(pool.totalAllocated(), 0);
}

QTEST_MAIN(TestVideoFramePool)
#include "test_video_frame_pool.moc"
