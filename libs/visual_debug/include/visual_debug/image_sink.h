#pragma once

#include "visual_debug/image_message.h"

namespace visual_debug {

// Receives decoded images from a Server. Called on the server's own thread: an implementation hands the
// image over to its UI thread itself.
class ImageSink {
public:
    virtual ~ImageSink() = default;

    // Returns false when the image cannot be taken now (the viewer is busy); the sender then sees Rejected.
    virtual bool onImageReceived(ImageMessage message) = 0;
};

} // namespace visual_debug
