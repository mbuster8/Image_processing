#include "viewer_core/viewport.h"

#include <algorithm>
#include <cmath>

namespace viewer_core {

void Viewport::setImageSize(cv::Size size) {
    m_imageSize = cv::Size2d(size.width, size.height);
    fit();
}

void Viewport::setViewSize(cv::Size2d size) {
    m_viewSize = size;
    m_zoom = std::clamp(m_zoom, 1.0, maxZoom());
    clampCentre();
}

void Viewport::fit() {
    m_zoom = 1.0;
    m_centre = cv::Point2d(m_imageSize.width / 2, m_imageSize.height / 2);
}

void Viewport::zoomAt(cv::Point2d viewPoint, double steps) {
    if (!hasArea()) {
        return;
    }
    const cv::Point2d anchor = viewToImage(viewPoint);
    m_zoom = std::clamp(m_zoom * std::pow(kZoomStep, steps), 1.0, maxZoom());
    // Choose the centre that puts `anchor` back under `viewPoint` at the new scale.
    m_centre = anchor - (viewPoint - viewCentre()) / scale();
    clampCentre();
}

void Viewport::panBy(cv::Point2d viewDelta) {
    if (!hasArea()) {
        return;
    }
    m_centre -= viewDelta / scale();
    clampCentre();
}

double Viewport::zoom() const { return m_zoom; }

double Viewport::scale() const { return fitScale() * m_zoom; }

cv::Point2d Viewport::viewToImage(cv::Point2d viewPoint) const {
    if (!hasArea()) {
        return m_centre;
    }
    return m_centre + (viewPoint - viewCentre()) / scale();
}

cv::Point2d Viewport::imageToView(cv::Point2d imagePoint) const {
    return viewCentre() + (imagePoint - m_centre) * scale();
}

cv::Rect2d Viewport::visibleImageRect() const {
    if (!hasArea()) {
        return {m_centre, cv::Size2d(0, 0)};
    }
    const cv::Rect2d visible(viewToImage({0, 0}), viewToImage({m_viewSize.width, m_viewSize.height}));
    return visible & cv::Rect2d(0, 0, m_imageSize.width, m_imageSize.height);
}

// Zero sizes (no image yet, a minimised window) would otherwise divide by zero and spread NaN everywhere.
bool Viewport::hasArea() const {
    return m_imageSize.width > 0 && m_imageSize.height > 0 && m_viewSize.width > 0 && m_viewSize.height > 0;
}

double Viewport::fitScale() const {
    if (!hasArea()) {
        return 0.0;
    }
    return std::min(m_viewSize.width / m_imageSize.width, m_viewSize.height / m_imageSize.height);
}

double Viewport::maxZoom() const {
    if (!hasArea()) {
        return 1.0;
    }
    const double maxScale = std::min(m_viewSize.width, m_viewSize.height) / kMinVisiblePixels;
    return std::max(1.0, maxScale / fitScale());
}

cv::Point2d Viewport::viewCentre() const { return {m_viewSize.width / 2, m_viewSize.height / 2}; }

// Along each axis: when the image is narrower than the view it stays centred; otherwise the view may not
// show anything beyond the image's edge.
void Viewport::clampCentre() {
    if (!hasArea()) {
        return;
    }
    const auto clampAxis = [](double centre, double visibleExtent, double imageExtent) {
        if (visibleExtent >= imageExtent) {
            return imageExtent / 2;
        }
        return std::clamp(centre, visibleExtent / 2, imageExtent - visibleExtent / 2);
    };
    m_centre.x = clampAxis(m_centre.x, m_viewSize.width / scale(), m_imageSize.width);
    m_centre.y = clampAxis(m_centre.y, m_viewSize.height / scale(), m_imageSize.height);
}

} // namespace viewer_core
