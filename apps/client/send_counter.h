#pragma once

#include "visual_debug/image_sender.h"

#include <string>
#include <string_view>

// Numbers the images the client sends, so the viewer's history tells repeated sends of one file apart. A
// number is used up only by an image the viewer may have taken: one that timed out may still have arrived,
// so its number is not reused, while a send that failed or was rejected keeps its number for the next.
class SendCounter {
public:
    std::string nameFor(std::string_view fileName) const {
        return std::string(fileName) + " #" + std::to_string(m_next);
    }

    void record(visual_debug::SendResult result) {
        if (result == visual_debug::SendResult::Delivered || result == visual_debug::SendResult::Timeout) {
            ++m_next;
        }
    }

private:
    int m_next = 1;
};
