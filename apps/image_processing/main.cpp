#include "main_window.h"

#include "visual_debug/endpoint.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Image Processing"));
    QApplication::setApplicationVersion(QStringLiteral(IMAGE_PROCESSING_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/application_icon.png")));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Shows images sent by programs that use the visual_debug library."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption endpointOption(
        QStringLiteral("endpoint"),
        QStringLiteral("ZeroMQ endpoint to listen on (default: $VISUAL_DEBUG_ENDPOINT, else tcp://127.0.0.1:7139)."),
        QStringLiteral("uri"), QString::fromStdString(visual_debug::defaultEndpoint()));
    const QCommandLineOption allowRemoteOption(
        QStringLiteral("allow-remote"),
        QStringLiteral("Accept connections from other machines. Images travel unencrypted and unauthenticated."));
    parser.addOptions({endpointOption, allowRemoteOption});
    parser.process(application);

    MainWindow window(visual_debug::ServerOptions{.endpoint = parser.value(endpointOption).toStdString(),
                                                  .allowRemote = parser.isSet(allowRemoteOption)});
    window.showMaximized();
    return QApplication::exec();
}
