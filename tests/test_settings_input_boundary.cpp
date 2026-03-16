#include <QtTest/QtTest>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>

#include "ui/SettingsInputBoundary.hpp"

namespace {

class InteractiveItem : public QQuickItem {
    Q_OBJECT

public:
    using QQuickItem::QQuickItem;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        event->accept();
    }
};

class BoundaryFixture {
public:
    BoundaryFixture()
        : boundary(window.contentItem())
    {
        window.resize(240, 180);
        window.show();
        QTest::qWait(10);

        boundary.setParentItem(window.contentItem());
        boundary.setWidth(window.width());
        boundary.setHeight(window.height());
    }

    QQuickWindow window;
    oap::SettingsInputBoundary boundary;
};

InteractiveItem* makeInteractiveChild(QQuickItem* parent, const QRectF& rect)
{
    auto* item = new InteractiveItem(parent);
    item->setParentItem(parent);
    item->setAcceptedMouseButtons(Qt::LeftButton);
    item->setAcceptTouchEvents(true);
    item->setX(rect.x());
    item->setY(rect.y());
    item->setWidth(rect.width());
    item->setHeight(rect.height());
    return item;
}

} // namespace

class TestSettingsInputBoundary : public QObject {
    Q_OBJECT

private slots:
    void testLongPressFiresForInteractiveChild();
    void testLongPressSkippedForBlockingChild();
    void testLongPressSkippedForBlockingAncestor();
};

void TestSettingsInputBoundary::testLongPressFiresForInteractiveChild()
{
    BoundaryFixture fixture;
    makeInteractiveChild(&fixture.boundary, QRectF(20, 20, 120, 80));

    QSignalSpy pressSpy(&fixture.boundary, &oap::SettingsInputBoundary::pressStarted);
    QSignalSpy longSpy(&fixture.boundary, &oap::SettingsInputBoundary::longPressed);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
    QTRY_COMPARE_WITH_TIMEOUT(pressSpy.count(), 1, 100);
    QTRY_COMPARE_WITH_TIMEOUT(longSpy.count(), 1, 700);
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
}

void TestSettingsInputBoundary::testLongPressSkippedForBlockingChild()
{
    BoundaryFixture fixture;
    auto* child = makeInteractiveChild(&fixture.boundary, QRectF(20, 20, 120, 80));
    child->setProperty("blocksBackHold", true);

    QSignalSpy pressSpy(&fixture.boundary, &oap::SettingsInputBoundary::pressStarted);
    QSignalSpy longSpy(&fixture.boundary, &oap::SettingsInputBoundary::longPressed);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
    QTest::qWait(650);
    QCOMPARE(pressSpy.count(), 0);
    QCOMPARE(longSpy.count(), 0);
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
}

void TestSettingsInputBoundary::testLongPressSkippedForBlockingAncestor()
{
    BoundaryFixture fixture;

    QQuickItem container(&fixture.boundary);
    container.setParentItem(&fixture.boundary);
    container.setProperty("blocksBackHold", true);
    container.setX(20);
    container.setY(20);
    container.setWidth(120);
    container.setHeight(80);

    makeInteractiveChild(&container, QRectF(0, 0, 120, 80));

    QSignalSpy pressSpy(&fixture.boundary, &oap::SettingsInputBoundary::pressStarted);
    QSignalSpy longSpy(&fixture.boundary, &oap::SettingsInputBoundary::longPressed);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
    QTest::qWait(650);
    QCOMPARE(pressSpy.count(), 0);
    QCOMPARE(longSpy.count(), 0);
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
}

QTEST_MAIN(TestSettingsInputBoundary)

#include "test_settings_input_boundary.moc"
