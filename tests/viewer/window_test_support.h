#pragma once

#include "main_window.h"

#include "visual_debug/server.h"

#include <QString>
#include <QTemporaryDir>

#include <memory>

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

} // namespace test_support
