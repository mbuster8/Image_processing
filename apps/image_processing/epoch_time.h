#pragma once

#include <QtGlobal> // qint64; <QtTypes> is Qt 6.5 and newer

#include <chrono>
#include <optional>

// Times on disk are microseconds since the epoch: finer than QDateTime's milliseconds, and as fine as
// system_clock on macOS.
inline qint64 toEpochMicroseconds(std::chrono::system_clock::time_point time) {
    return std::chrono::floor<std::chrono::microseconds>(time.time_since_epoch()).count();
}

// Nullopt beyond what system_clock holds, where converting would overflow: a damaged file can say anything.
inline std::optional<std::chrono::system_clock::time_point> fromEpochMicroseconds(qint64 microseconds) {
    constexpr qint64 kLimit =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::duration::max())
            .count();
    if (microseconds > kLimit || microseconds < -kLimit) {
        return std::nullopt;
    }
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::microseconds(microseconds)));
}
