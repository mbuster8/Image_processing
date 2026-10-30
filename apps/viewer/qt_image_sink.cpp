#include "qt_image_sink.h"

#include <utility>

QtImageSink::QtImageSink(QObject* parent) : QObject(parent) {
    qRegisterMetaType<visual_debug::ImageMessage>(); // queued connections copy the argument by its metatype
}

visual_debug::Acceptance QtImageSink::onImageReceived(visual_debug::ImageMessage message) {
    {
        std::unique_lock lock(m_mutex);
        if (!m_slotFreed.wait_for(lock, kMaxWaitForUi, [this] { return m_pending < kMaxPendingImages; })) {
            return visual_debug::Acceptance::Busy;
        }
        ++m_pending;
    }
    emit imageReceived(std::move(message));
    return visual_debug::Acceptance::Taken;
}

void QtImageSink::markHandled() {
    {
        const std::lock_guard lock(m_mutex);
        if (m_pending > 0) {
            --m_pending;
        }
    }
    m_slotFreed.notify_one();
}

void QtImageSink::onRequestRejected(std::string_view reason) {
    bool first = false;
    {
        const std::lock_guard lock(m_mutex);
        first = m_rejections.empty();
        ++m_rejections[std::string(reason)];
    }
    if (first) {
        emit requestsRejected();
    }
}

std::map<std::string, int> QtImageSink::takeRejections() {
    const std::lock_guard lock(m_mutex);
    return std::exchange(m_rejections, {});
}
