#include "qt_image_sink.h"

#include <utility>

QtImageSink::QtImageSink(QObject* parent) : QObject(parent) {
    qRegisterMetaType<visual_debug::ImageMessage>(); // queued connections copy the argument by its metatype
}

bool QtImageSink::onImageReceived(visual_debug::ImageMessage message) {
    {
        std::unique_lock lock(m_mutex);
        if (!m_slotFreed.wait_for(lock, kMaxWaitForUi, [this] { return m_pending < kMaxPendingImages; })) {
            return false;
        }
        ++m_pending;
    }
    emit imageReceived(std::move(message));
    return true;
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
