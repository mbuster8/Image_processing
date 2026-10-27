#pragma once

#include "viewer_core/viewport.h"

#include <QImage>
#include <QPointF>
#include <QWidget>

#include <optional>

// Shows one image: wheel to zoom at the cursor, drag to pan, double-click to fit. Reports the image pixel
// under the cursor, so the window can show its original values.
class ImageView : public QWidget {
    Q_OBJECT

public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& image); // shown fitted to the view
    void clear();

signals:
    void pixelHovered(int x, int y);
    void hoverLeft();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void reportPixelAt(QPointF viewPoint);

    QImage m_image;
    viewer_core::Viewport m_viewport;
    std::optional<QPointF> m_lastDragPosition;
    std::optional<QPointF> m_hoverPosition; // while the cursor is over the view
};
