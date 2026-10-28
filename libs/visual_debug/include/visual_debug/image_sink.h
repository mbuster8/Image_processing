#pragma once

#include "visual_debug/image_message.h"

#include <string_view>

namespace visual_debug {

// Receives decoded images from a Server. Called on the server's own thread: an implementation hands the
// image over to its UI thread itself.
class ImageSink {
public:
    virtual ~ImageSink() = default;

    // Returns false when the image cannot be taken now (the viewer is busy); the sender then sees Rejected.
    virtual bool onImageReceived(ImageMessage message) = 0;

    // Called for every request the server answers with an error, with the reply's fixed ASCII reason (never
    // request bytes). Runs on the server's thread just before the reply is sent, so it must return at once.
    virtual void onRequestRejected(std::string_view /*reason*/) {}
};

} // namespace visual_debug
