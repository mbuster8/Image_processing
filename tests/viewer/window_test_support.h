#pragma once

#include "main_window.h"

#include "visual_debug/server.h"

#include <QString>
#include <QTemporaryDir>

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

namespace test_support {

// A data root for the whole run, so a test never touches the owner's real viewer data.
inline QString testDataRoot() {
    static const QTemporaryDir root;
    return root.path();
}

// A viewer window on an ephemeral loopback port, so tests never touch 7139 and can run beside a real
// viewer.
inline std::unique_ptr<MainWindow>
makeTestWindow(const QString& endpoint = QStringLiteral("tcp://127.0.0.1:*"),
               const QString& dataRoot = testDataRoot()) {
    return std::make_unique<MainWindow>(visual_debug::ServerOptions{.endpoint = endpoint.toStdString()},
                                        dataRoot);
}

// The same on a clock the test moves, so live channels go idle and rates change without waiting.
inline std::unique_ptr<MainWindow>
makeClockedTestWindow(std::function<std::chrono::steady_clock::time_point()> clock) {
    return std::make_unique<MainWindow>(visual_debug::ServerOptions{.endpoint = "tcp://127.0.0.1:*"},
                                        testDataRoot(), std::move(clock));
}

} // namespace test_support
