#include "image_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

#include <cmath>

namespace {

cv::Point2d toCv(QPointF point) { return {point.x(), point.y()}; }

QPointF toQt(cv::Point2d point) { return {point.x, point.y}; }

} // namespace

ImageView::ImageView(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true); // pixel readout while hovering, not only while dragging
    setMinimumSize(200, 200);
}

void ImageView::setImage(const QImage& image) {
    m_image = image;
    m_viewport.setImageSize({image.width(), image.height()});
    update();
    // A new image under a resting cursor: report its pixel, or the readout would show the old image's.
    if (m_hoverPosition) {
        reportPixelAt(*m_hoverPosition);
    }
}

void ImageView::clear() {
    m_image = QImage();
    m_viewport.setImageSize({0, 0});
    update();
}

void ImageView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Dark));
    if (m_image.isNull()) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("No image yet.\nSend one with visual_debug::sendImage(), or use File → Open…"));
        return;
    }
    // Smooth when shrinking; nearest-neighbour when enlarging, so individual pixels stay visible.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_viewport.scale() < 1.0);
    const cv::Rect2d source = m_viewport.visibleImageRect();
    const QRectF target(toQt(m_viewport.imageToView(source.tl())), toQt(m_viewport.imageToView(source.br())));
    painter.drawImage(target, m_image, QRectF(source.x, source.y, source.width, source.height));
}

void ImageView::resizeEvent(QResizeEvent* event) {
    m_viewport.setViewSize({static_cast<double>(width()), static_cast<double>(height())});
    QWidget::resizeEvent(event);
}

void ImageView::wheelEvent(QWheelEvent* event) {
    if (m_image.isNull()) {
        return;
    }
    // 120 units is one notch of a mouse wheel; trackpads send smaller deltas, which zoom proportionally.
    m_viewport.zoomAt(toCv(event->position()), event->angleDelta().y() / 120.0);
    update();
    // Zoom keeps the point under the cursor, except where it stops at the fit and re-centres the image.
    reportPixelAt(event->position());
    event->accept();
}

void ImageView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        m_lastDragPosition = event->position();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageView::mouseMoveEvent(QMouseEvent* event) {
    m_hoverPosition = event->position();
    if (m_lastDragPosition) {
        m_viewport.panBy(toCv(event->position() - *m_lastDragPosition));
        m_lastDragPosition = event->position();
        update();
    }
    reportPixelAt(event->position());
}

void ImageView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_lastDragPosition.reset();
        unsetCursor();
    }
}

void ImageView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_viewport.fit();
        update();
        reportPixelAt(event->position()); // fitting moves the image under the cursor
    }
}

void ImageView::leaveEvent(QEvent* event) {
    m_hoverPosition.reset();
    emit hoverLeft();
    QWidget::leaveEvent(event);
}

void ImageView::reportPixelAt(QPointF viewPoint) {
    if (m_image.isNull()) {
        return;
    }
    // Floor, not truncation: the point at -0.5 lies in pixel -1 (outside the image), not in pixel 0.
    const cv::Point2d imagePoint = m_viewport.viewToImage(toCv(viewPoint));
    emit pixelHovered(static_cast<int>(std::floor(imagePoint.x)), static_cast<int>(std::floor(imagePoint.y)));
}
