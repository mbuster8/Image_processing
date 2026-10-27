#include "visual_debug/image_sender.h"

namespace visual_debug {

std::string_view toString(SendResult result) {
    switch (result) {
    case SendResult::Delivered:
        return "Delivered";
    case SendResult::ViewerUnavailable:
        return "Viewer unavailable";
    case SendResult::Rejected:
        return "Rejected";
    case SendResult::InvalidImage:
        return "Invalid image";
    case SendResult::Timeout:
        return "Timeout";
    case SendResult::InvalidEndpoint:
        return "Invalid endpoint";
    }
    return "Unknown";
}

} // namespace visual_debug
