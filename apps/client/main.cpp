#include "main_window.h"

#include "visual_debug/client.h"
#include "visual_debug/endpoint.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Image Processing Client"));

    // Sends to $VISUAL_DEBUG_ENDPOINT, else the viewer's default endpoint.
    visual_debug::Client client(visual_debug::ClientOptions{.endpoint = visual_debug::defaultEndpoint()});
    MainWindow window(client);
    window.show();
    return QApplication::exec();
}
