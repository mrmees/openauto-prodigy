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
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
    void testRecycledFrameIsValid();
    void testRecycledBufferReturnsToPool();
    void testRecycledPoolGrowsOnDemand();
    void testRecycledFormatChange();
#endif
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

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
void TestVideoFramePool::testRecycledFrameIsValid()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    QVideoFrame frame = pool.acquireRecycled();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.size(), QSize(1280, 720));

    // Should be mappable
    QVERIFY(frame.map(QVideoFrame::ReadOnly));
    QVERIFY(frame.bits(0) != nullptr);
    QCOMPARE(frame.bytesPerLine(0), 1280);
    QCOMPARE(frame.bytesPerLine(1), 640);
    frame.unmap();
}

void TestVideoFramePool::testRecycledBufferReturnsToPool()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    QCOMPARE(pool.freeCount(), 0);
    QCOMPARE(pool.totalAllocated(), 0);

    // Acquire and immediately release
    { QVideoFrame frame = pool.acquireRecycled(); }

    // Buffer should be back in pool
    QCOMPARE(pool.freeCount(), 1);
    QCOMPARE(pool.totalAllocated(), 1);

    // Next acquire should recycle
    { QVideoFrame frame = pool.acquireRecycled(); }
    QCOMPARE(pool.totalAllocated(), 1);  // No new allocation
    QCOMPARE(pool.totalRecycled(), 1);
}

void TestVideoFramePool::testRecycledPoolGrowsOnDemand()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    // Hold multiple frames — pool must allocate new ones
    std::vector<QVideoFrame> held;
    for (int i = 0; i < 5; ++i) {
        held.push_back(pool.acquireRecycled());
        QVERIFY(held.back().isValid());
    }
    QCOMPARE(pool.totalAllocated(), 5);
    QCOMPARE(pool.freeCount(), 0);

    // Release all
    held.clear();
    QCOMPARE(pool.freeCount(), 5);
}

void TestVideoFramePool::testRecycledFormatChange()
{
    QVideoFrameFormat fmt720({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt720, 3);

    // Acquire and release to populate pool
    { QVideoFrame f = pool.acquireRecycled(); }
    QCOMPARE(pool.freeCount(), 1);

    // Reset to new resolution — should discard old buffers
    QVideoFrameFormat fmt1080({1920, 1080}, QVideoFrameFormat::Format_YUV420P);
    pool.reset(fmt1080);
    QCOMPARE(pool.freeCount(), 0);
    QCOMPARE(pool.totalAllocated(), 0);

    QVideoFrame frame = pool.acquireRecycled();
    QCOMPARE(frame.size(), QSize(1920, 1080));
    QCOMPARE(pool.totalAllocated(), 1);
}
#endif

QTEST_MAIN(TestVideoFramePool)
#include "test_video_frame_pool.moc"
