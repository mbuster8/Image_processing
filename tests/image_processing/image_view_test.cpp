#include "image_view.h"

#include <QCoreApplication>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QWheelEvent>

#include <gtest/gtest.h>

namespace {

QImage filledImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::gray);
    return image;
}

void sendMouseEvent(ImageView& view, QEvent::Type type, QPointF position, Qt::MouseButton button) {
    QMouseEvent event(type, position, view.mapToGlobal(position), button, button, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &event);
}

// 120 units is one notch of a mouse wheel; positive zooms in.
void sendWheelEvent(ImageView& view, QPointF position, int angleDelta) {
    QWheelEvent event(position, view.mapToGlobal(position), QPoint(), QPoint(0, angleDelta), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&view, &event);
}

// A 200×200 view: a 100×100 image fits at scale 2, a 400×400 one at scale 0.5.
class ImageViewTest : public testing::Test {
protected:
    void SetUp() override {
        m_view.resize(200, 200);
        m_view.show();
        m_view.setImage(filledImage(100, 100));
    }

    ImageView m_view;
};

TEST_F(ImageViewTest, ReportsPixelUnderRestingCursorWhenImageChanges) {
    QSignalSpy hovered(&m_view, &ImageView::pixelHovered);
    sendMouseEvent(m_view, QEvent::MouseMove, {100, 100}, Qt::NoButton);
    ASSERT_EQ(hovered.size(), 1);

    m_view.setImage(filledImage(400, 400)); // a new image arrives; the cursor has not moved

    ASSERT_EQ(hovered.size(), 2);
    EXPECT_EQ(hovered.last().at(0).toInt(), 200); // the centre of the new image, not the old (50, 50)
    EXPECT_EQ(hovered.last().at(1).toInt(), 200);
}

TEST_F(ImageViewTest, ReportsPixelUnderCursorAfterFitting) {
    sendMouseEvent(m_view, QEvent::MouseMove, {150, 150}, Qt::NoButton);
    QSignalSpy hovered(&m_view, &ImageView::pixelHovered);

    sendMouseEvent(m_view, QEvent::MouseButtonDblClick, {150, 150}, Qt::LeftButton);

    EXPECT_EQ(hovered.size(), 1);
}

TEST_F(ImageViewTest, ReportsPixelUnderCursorAfterZoomingOutToFit) {
    sendWheelEvent(m_view, {20, 20}, 4 * 120); // zoomed in around image point (10, 10)
    sendMouseEvent(m_view, QEvent::MouseMove, {180, 180}, Qt::NoButton); // over image pixel (42, 42)
    QSignalSpy hovered(&m_view, &ImageView::pixelHovered);

    // Zooming out stops at the fit, which centres the image, so a different pixel ends up under the cursor.
    sendWheelEvent(m_view, {180, 180}, -10 * 120);

    ASSERT_EQ(hovered.size(), 1);
    EXPECT_EQ(hovered.last().at(0).toInt(), 90);
    EXPECT_EQ(hovered.last().at(1).toInt(), 90);
}

} // namespace
