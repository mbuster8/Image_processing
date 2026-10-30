#pragma once

#include <opencv2/core/types.hpp>

namespace viewer_core {

// Zoom and pan arithmetic for showing an image in a view, independent of any UI toolkit.
// "View" coordinates are the widget's pixels; "image" coordinates are the image's pixels, with (0, 0) at the
// top-left corner of the first pixel. The smallest zoom fits the whole image, centred; the largest leaves
// at least kMinVisiblePixels image pixels across the view.
class Viewport {
public:
    static constexpr double kZoomStep = 1.25; // per mouse-wheel notch
    static constexpr double kMinVisiblePixels = 3.0;

    void setImageSize(cv::Size size);  // and fit
    void setViewSize(cv::Size2d size); // keeps the image point at the centre of the view
    void fit();
    // Zooms by kZoomStep^steps, keeping the image point under viewPoint where it is. Steps may be
    // fractional (trackpads send small wheel deltas).
    void zoomAt(cv::Point2d viewPoint, double steps);
    // Moves the image with the cursor, never further than its edges.
    void panBy(cv::Point2d viewDelta);

    double zoom() const;  // relative to fit; 1 is fitted
    double scale() const; // view pixels per image pixel
    cv::Point2d viewToImage(cv::Point2d viewPoint) const;
    cv::Point2d imageToView(cv::Point2d imagePoint) const;
    cv::Rect2d visibleImageRect() const; // clipped to the image

private:
    bool hasArea() const;
    double fitScale() const;
    double maxZoom() const;
    cv::Point2d viewCentre() const;
    void clampCentre();

    cv::Size2d m_imageSize{0, 0};
    cv::Size2d m_viewSize{0, 0};
    double m_zoom = 1.0;
    cv::Point2d m_centre{0, 0}; // the image point shown at the centre of the view
};

} // namespace viewer_core
