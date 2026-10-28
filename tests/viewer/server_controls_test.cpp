#include "server_controls.h"

#include "control_checks.h"

#include <QColor>
#include <QImage>
#include <QPalette>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QToolButton>

#include <gtest/gtest.h>

namespace {

QStringList visibleButtons(const ServerControls& controls) {
    QStringList texts;
    for (const QToolButton* button : controls.findChildren<QToolButton*>()) {
        if (!button->isHidden()) {
            texts << button->text();
        }
    }
    return texts;
}

QToolButton* buttonNamed(const ServerControls& controls, const QString& text) {
    for (QToolButton* button : controls.findChildren<QToolButton*>()) {
        if (!button->isHidden() && button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

QColor dotCentre(const ServerControls& controls) {
    auto* dot = controls.findChild<QWidget*>(QStringLiteral("stateDot"));
    const QPixmap grabbed = dot->grab();
    return grabbed.toImage().pixelColor(grabbed.width() / 2, grabbed.height() / 2);
}

// Large enough to see (spec H1): framed buttons with 18-point icons, 28 points high, and a 12-point dot.
TEST(ServerControls, ControlsAreLargeEnoughToSee) {
    ServerControls controls;
    controls.setState(ServerState::Listening, QStringLiteral("Listening on [tcp://127.0.0.1:7139]"));
    controls.show();

    EXPECT_TRUE(test_support::isLargeFramedButton(buttonNamed(controls, QStringLiteral("Pause"))));
    EXPECT_TRUE(test_support::isLargeFramedButton(buttonNamed(controls, QStringLiteral("Stop"))));
    EXPECT_EQ(controls.findChild<QWidget*>(QStringLiteral("stateDot"))->size(), QSize(12, 12));
}

TEST(ServerControls, ButtonsPerState) {
    ServerControls controls;

    controls.setState(ServerState::Listening, QStringLiteral("Listening on [tcp://127.0.0.1:7139]"));
    EXPECT_EQ(visibleButtons(controls), (QStringList{QStringLiteral("Pause"), QStringLiteral("Stop")}));
    EXPECT_EQ(controls.state(), ServerState::Listening);
    EXPECT_EQ(controls.text(), QStringLiteral("Listening on [tcp://127.0.0.1:7139]"));

    controls.setState(ServerState::Paused, QString());
    EXPECT_EQ(visibleButtons(controls), (QStringList{QStringLiteral("Resume"), QStringLiteral("Stop")}));

    controls.setState(ServerState::Stopped, QString());
    EXPECT_EQ(visibleButtons(controls), QStringList{QStringLiteral("Start")});

    controls.setState(ServerState::NotListening, QString());
    EXPECT_EQ(visibleButtons(controls), QStringList{QStringLiteral("Try again")});

    controls.setState(ServerState::Starting, QString());
    EXPECT_TRUE(visibleButtons(controls).isEmpty());
}

TEST(ServerControls, ButtonsEmitTheirSignals) {
    ServerControls controls;
    QSignalSpy pause(&controls, &ServerControls::pauseClicked);
    QSignalSpy stop(&controls, &ServerControls::stopClicked);

    controls.setState(ServerState::Listening, QString());
    QTest::mouseClick(buttonNamed(controls, QStringLiteral("Pause")), Qt::LeftButton);
    QTest::mouseClick(buttonNamed(controls, QStringLiteral("Stop")), Qt::LeftButton);
    controls.setState(ServerState::Stopped, QString());
    QTest::mouseClick(buttonNamed(controls, QStringLiteral("Start")), Qt::LeftButton);

    EXPECT_EQ(pause.size(), 1);
    EXPECT_EQ(stop.size(), 2);
}

// The dot is the only colour: green while listening, amber while paused, a hollow grey ring otherwise.
TEST(ServerControls, DotShowsTheState) {
    ServerControls controls;

    controls.setState(ServerState::Listening, QString());
    EXPECT_EQ(dotCentre(controls), QColor(0x2e, 0x9e, 0x4f));

    controls.setState(ServerState::Paused, QString());
    EXPECT_EQ(dotCentre(controls), QColor(0xd9, 0x9a, 0x00));

    controls.setState(ServerState::Stopped, QString());
    const QColor hollow = dotCentre(controls);
    EXPECT_NE(hollow, QColor(0x2e, 0x9e, 0x4f));
    EXPECT_NE(hollow, QColor(0xd9, 0x9a, 0x00));
    EXPECT_NE(hollow, controls.palette().color(QPalette::Mid));
}

} // namespace
