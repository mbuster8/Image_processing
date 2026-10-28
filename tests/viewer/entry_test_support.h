#pragma once

#include "viewer_core/display_converter.h"
#include "viewer_core/image_history.h"

#include <opencv2/core.hpp>

#include <string>
#include <utility>

namespace test_support {

// A history entry as the sidebar receives it: a rows × cols 8-bit grey image, with its display image and
// summary.
inline viewer_core::ReceivedImage makeEntry(viewer_core::ImageId id, std::string name, int rows = 3,
                                            int cols = 4) {
    const cv::Mat original(rows, cols, CV_8UC1, cv::Scalar(9));
    return {id, std::move(name), original, viewer_core::toDisplayImage(original),
            viewer_core::describeImage(original)};
}

} // namespace test_support
