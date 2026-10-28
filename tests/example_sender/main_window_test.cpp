#include "control_checks.h"
#include "main_window.h"
#include "shades.h"

#include "visual_debug/endpoint.h"

#include <QAction>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPalette>
#include <QPoint>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QTemporaryDir>
#include <QToolButton>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using visual_debug::SendResult;

// Senders that answer with `result` and record which endpoints the window made them for, and what it sent.
class FakeSenders {
public:
    SendResult result = SendResult::Delivered;
    std::vector<std::string> endpoints;
    std::vector<std::string> names;

    MainWindow::SenderFactory factory() {
        return [this](const std::string& endpoint) -> std::unique_ptr<visual_debug::ImageSender> {
            endpoints.push_back(endpoint);
            return std::make_unique<Sender>(*this);
        };
    }

private:
    class Sender final : public visual_debug::ImageSender {
    public:
        explicit Sender(FakeSenders& owner) : m_owner(owner) {}
        SendResult send(const cv::Mat& /*image*/, std::string_view name) override {
            m_owner.names.emplace_back(name);
            return m_owner.result;
        }

    private:
        FakeSenders& m_owner;
    };
};

// Sets VISUAL_DEBUG_ENDPOINT for one test, and removes it again however the test ends.
class EnvironmentEndpoint {
public:
    explicit EnvironmentEndpoint(const QByteArray& endpoint) {
        qputenv(visual_debug::kEndpointEnvironmentVariable, endpoint);
    }
    ~EnvironmentEndpoint() { qunsetenv(visual_debug::kEndpointEnvironmentVariable); }
    EnvironmentEndpoint(const EnvironmentEndpoint&) = delete;
    EnvironmentEndpoint& operator=(const EnvironmentEndpoint&) = delete;
};

const QString kRememberedKey = QStringLiteral("sender/endpoint");

QLineEdit* endpointFieldOf(MainWindow& window) {
    return window.findChild<QLineEdit*>(QStringLiteral("endpointField"));
}

QAction* sendActionOf(MainWindow& window) { return window.findChild<QAction*>(QStringLiteral("actionSend")); }

// The row along the top that holds Open, Send image and the endpoint field.
QWidget* commandBarOf(MainWindow& window) { return window.findChild<QWidget*>(QStringLiteral("commandBar")); }

QToolButton* buttonOf(MainWindow& window, const QString& objectName) {
    return window.findChild<QToolButton*>(objectName);
}

QString writeImage(const QTemporaryDir& directory, const QString& fileName) {
    const QString path = QDir(directory.path()).filePath(fileName);
    cv::imwrite(QFile::encodeName(path).toStdString(), cv::Mat(4, 6, CV_8UC3, cv::Scalar(1, 2, 3)));
    return path;
}

TEST(ExampleSenderWindow, TitleNamesTheApp) {
    FakeSenders senders;
    MainWindow window(senders.factory());

    EXPECT_EQ(window.windowTitle(), QStringLiteral("Visual Debug Example Sender"));
}

TEST(ExampleSenderWindow, StartsFromTheDefaultEndpoint) {
    FakeSenders senders;
    MainWindow window(senders.factory());

    ASSERT_NE(endpointFieldOf(window), nullptr);
    EXPECT_EQ(endpointFieldOf(window)->text(), QString::fromUtf8(visual_debug::kDefaultEndpoint.data(),
                                                                 visual_debug::kDefaultEndpoint.size()));
}

TEST(ExampleSenderWindow, StartsFromTheRememberedEndpoint) {
    QSettings().setValue(kRememberedKey, QStringLiteral("tcp://127.0.0.1:7150"));
    FakeSenders senders;
    MainWindow window(senders.factory());

    EXPECT_EQ(endpointFieldOf(window)->text(), QStringLiteral("tcp://127.0.0.1:7150"));
}

TEST(ExampleSenderWindow, EnvironmentEndpointWinsOverTheRememberedOne) {
    QSettings().setValue(kRememberedKey, QStringLiteral("tcp://127.0.0.1:7150"));
    const EnvironmentEndpoint environment("tcp://127.0.0.1:7160");
    FakeSenders senders;
    MainWindow window(senders.factory());

    EXPECT_EQ(endpointFieldOf(window)->text(), QStringLiteral("tcp://127.0.0.1:7160"));
}

// The field sits in the toolbar, beside Open and Send image.
TEST(ExampleSenderWindow, EndpointFieldIsInTheToolbar) {
    FakeSenders senders;
    MainWindow window(senders.factory());

    ASSERT_NE(commandBarOf(window), nullptr);
    EXPECT_EQ(endpointFieldOf(window)->parentWidget(), commandBarOf(window));
}

// Open and Send image show the shared outline icons beside their text.
TEST(ExampleSenderWindow, ToolbarShowsIconsBesideText) {
    FakeSenders senders;
    MainWindow window(senders.factory());
    QToolButton* open = buttonOf(window, QStringLiteral("openButton"));
    QToolButton* send = buttonOf(window, QStringLiteral("sendButton"));
    ASSERT_NE(open, nullptr);
    ASSERT_NE(send, nullptr);

    EXPECT_EQ(open->defaultAction(), window.findChild<QAction*>(QStringLiteral("actionOpen")));
    EXPECT_EQ(send->defaultAction(), sendActionOf(window));
    for (const QToolButton* button : {open, send}) {
        EXPECT_EQ(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
        EXPECT_FALSE(button->icon().isNull());
    }
}

// Large enough to see (spec H1), in the regular font even where the platform gives tool buttons a smaller
// one.
TEST(ExampleSenderWindow, ToolbarIsLargeEnoughToSee) {
    const test_support::SmallerClassFonts smaller;
    FakeSenders senders;
    MainWindow window(senders.factory());
    window.show();
    QToolButton* open = buttonOf(window, QStringLiteral("openButton"));
    QToolButton* send = buttonOf(window, QStringLiteral("sendButton"));

    EXPECT_TRUE(test_support::isLargeFramedButton(open));
    EXPECT_TRUE(test_support::isLargeFramedButton(send));
    EXPECT_TRUE(test_support::usesTheRegularFont(open));
    EXPECT_TRUE(test_support::isTallEnough(endpointFieldOf(window)));
}

// The toolbar sits on the panel grey, with a divider line along its bottom (spec H3, H8).
TEST(ExampleSenderWindow, ToolbarSitsOnThePanelGrey) {
    FakeSenders senders;
    MainWindow window(senders.factory());
    window.show();
    QWidget* toolBar = commandBarOf(window);
    auto* divider = window.findChild<QWidget*>(QStringLiteral("toolBarDivider"));
    ASSERT_NE(toolBar, nullptr);
    ASSERT_NE(divider, nullptr);

    EXPECT_TRUE(toolBar->autoFillBackground());
    EXPECT_EQ(toolBar->palette().color(QPalette::Window), qt_common::panelGrey(QGuiApplication::palette()));
    EXPECT_EQ(divider->height(), 1);
    EXPECT_EQ(divider->mapTo(&window, QPoint(0, 0)).y(),
              toolBar->mapTo(&window, QPoint(0, toolBar->height())).y());
}

TEST(ExampleSenderWindow, SendsToTheFieldsEndpointAndRemembersIt) {
    const QTemporaryDir directory;
    FakeSenders senders;
    MainWindow window(senders.factory());
    window.openImage(writeImage(directory, QStringLiteral("a.png")));
    endpointFieldOf(window)->setText(QStringLiteral(" tcp://127.0.0.1:7150 "));

    sendActionOf(window)->trigger();
    sendActionOf(window)->trigger();

    EXPECT_EQ(senders.endpoints, std::vector<std::string>{"tcp://127.0.0.1:7150"}); // one sender, reused
    EXPECT_EQ(senders.names, (std::vector<std::string>{"a.png #1", "a.png #2"}));
    EXPECT_EQ(window.statusBar()->currentMessage(), QStringLiteral("[a.png #2]: Delivered"));
    EXPECT_EQ(QSettings().value(kRememberedKey).toString(), QStringLiteral("tcp://127.0.0.1:7150"));

    endpointFieldOf(window)->setText(QStringLiteral("tcp://127.0.0.1:7151"));
    sendActionOf(window)->trigger();
    EXPECT_EQ(senders.endpoints, (std::vector<std::string>{"tcp://127.0.0.1:7150", "tcp://127.0.0.1:7151"}));
}

// An unusable endpoint is marked and not remembered; editing the field clears the mark.
TEST(ExampleSenderWindow, MarksAnUnusableEndpoint) {
    const QTemporaryDir directory;
    FakeSenders senders;
    senders.result = SendResult::InvalidEndpoint;
    MainWindow window(senders.factory());
    window.openImage(writeImage(directory, QStringLiteral("a.png")));
    QLineEdit* field = endpointFieldOf(window);
    EXPECT_FALSE(field->property("invalid").toBool());
    field->setText(QStringLiteral("tcp://localhost"));

    sendActionOf(window)->trigger();

    EXPECT_TRUE(field->property("invalid").toBool());
    EXPECT_EQ(window.statusBar()->currentMessage(), QStringLiteral("[a.png #1]: Invalid endpoint"));
    EXPECT_FALSE(QSettings().contains(kRememberedKey));

    field->setText(QStringLiteral("tcp://127.0.0.1:7139"));
    EXPECT_FALSE(field->property("invalid").toBool());
}

TEST(ExampleSenderWindow, ReportsOpenedFilesInBrackets) {
    const QTemporaryDir directory;
    FakeSenders senders;
    MainWindow window(senders.factory());

    window.openImage(writeImage(directory, QStringLiteral("my photo.png")));
    EXPECT_EQ(window.statusBar()->currentMessage(), QStringLiteral("Opened [my photo.png]"));
    EXPECT_TRUE(sendActionOf(window)->isEnabled());

    QFile text(QDir(directory.path()).filePath(QStringLiteral("notes.txt")));
    ASSERT_TRUE(text.open(QIODevice::WriteOnly));
    text.write("not an image");
    text.close();
    window.openImage(text.fileName());
    EXPECT_EQ(window.statusBar()->currentMessage(),
              QStringLiteral("Cannot open [notes.txt]: not a supported image"));
}

} // namespace
