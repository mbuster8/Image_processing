#include "appearance.h"
#include "application_identity.h"
#include "main_window.h"

#include "visual_debug/client.h"

#include <QApplication>
#include <QIcon>

#include <memory>
#include <string>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    qt_common::setApplicationIdentity(QLatin1String(qt_common::kExampleSenderName));
    qt_common::applyAppearance(qt_common::storedAppearance());
    QApplication::setApplicationVersion(QStringLiteral(ID_VISION_LAB_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/example_sender_icon.png")));

    MainWindow window([](const std::string& endpoint) -> std::unique_ptr<visual_debug::ImageSender> {
        return std::make_unique<visual_debug::Client>(visual_debug::ClientOptions{.endpoint = endpoint});
    });
    window.show();
    return QApplication::exec();
}
