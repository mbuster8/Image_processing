#include "display_settings_control.h"

#include "viewer_core/display_settings.h"

#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QToolButton>
#include <QWidget>

#include <gtest/gtest.h>

#include <vector>

namespace {

using viewer_core::ColourMap;
using viewer_core::DisplayRange;
using viewer_core::DisplaySettings;
using viewer_core::RangeMode;

QAction* actionTitled(const QMenu* menu, const QString& title) {
    for (QAction* action : menu->actions()) {
        if (action->text().remove(u'&') == title) {
            return action;
        }
    }
    return nullptr;
}

QString checkedTitle(const QMenu* menu) {
    for (const QAction* action : menu->actions()) {
        if (action->isChecked()) {
            return action->text().remove(u'&');
        }
    }
    return {};
}

// A shown control that keeps what it emits.
class DisplaySettingsControlTest : public testing::Test {
protected:
    void SetUp() override {
        QObject::connect(&m_control, &DisplaySettingsControl::settingsChanged,
                         [this](const DisplaySettings& settings) { m_emitted.push_back(settings); });
        m_control.setAutoRangeProvider([this] { return m_autoRange; });
        m_control.setState(DisplaySettings{}, true);
        m_control.show();
    }

    QToolButton* button(const char* name) {
        return m_control.findChild<QToolButton*>(QString::fromLatin1(name));
    }
    QWidget* popup() { return m_control.findChild<QWidget*>(QStringLiteral("fixedRangePopup")); }
    QLineEdit* field(const char* name) { return m_control.findChild<QLineEdit*>(QString::fromLatin1(name)); }
    QPushButton* applyButton() {
        return m_control.findChild<QPushButton*>(QStringLiteral("applyFixedRange"));
    }

    void openFixed() {
        actionTitled(m_control.rangeMenu(), QStringLiteral("Fixed…"))->trigger();
        ASSERT_NE(popup(), nullptr);
        ASSERT_TRUE(popup()->isVisible());
    }

    void type(const char* name, const QString& text) {
        field(name)->selectAll();
        QTest::keyClicks(field(name), text);
    }

    DisplaySettingsControl m_control;
    DisplayRange m_autoRange{10, 20};
    std::vector<DisplaySettings> m_emitted;
};

TEST_F(DisplaySettingsControlTest, StartsOnAutoAndGrey) {
    EXPECT_EQ(button("rangeButton")->text(), QStringLiteral("Range: Auto ▾"));
    EXPECT_EQ(button("coloursButton")->text(), QStringLiteral("Colours: Grey ▾"));
    EXPECT_EQ(checkedTitle(m_control.rangeMenu()), QStringLiteral("Auto"));
    EXPECT_EQ(checkedTitle(m_control.coloursMenu()), QStringLiteral("Grey"));
    QStringList ranges;
    for (const QAction* action : m_control.rangeMenu()->actions()) {
        ranges << action->text().remove(u'&');
    }
    EXPECT_EQ(ranges,
              (QStringList{QStringLiteral("Auto"), QStringLiteral("Hold"), QStringLiteral("Fixed…")}));
    EXPECT_TRUE(m_emitted.empty());
}

TEST_F(DisplaySettingsControlTest, ButtonsOpenTheirMenus) {
    button("rangeButton")->click();
    EXPECT_TRUE(m_control.rangeMenu()->isVisible());
    m_control.rangeMenu()->hide();

    button("coloursButton")->click();
    EXPECT_TRUE(m_control.coloursMenu()->isVisible());
    m_control.coloursMenu()->hide();
}

TEST_F(DisplaySettingsControlTest, HoldCapturesTheProvidedRange) {
    actionTitled(m_control.rangeMenu(), QStringLiteral("Hold"))->trigger();

    ASSERT_EQ(m_emitted.size(), 1u);
    EXPECT_EQ(m_emitted.back(), (DisplaySettings{.mode = RangeMode::Hold, .range = {10, 20}}));
    EXPECT_EQ(button("rangeButton")->text(), QStringLiteral("Range: Hold ▾"));
    EXPECT_EQ(checkedTitle(m_control.rangeMenu()), QStringLiteral("Hold"));

    actionTitled(m_control.rangeMenu(), QStringLiteral("Auto"))->trigger();
    ASSERT_EQ(m_emitted.size(), 2u);
    EXPECT_EQ(m_emitted.back(), DisplaySettings{});
}

TEST_F(DisplaySettingsControlTest, FixedRefusesAnEmptyRange) {
    openFixed();
    EXPECT_EQ(field("fixedMinimum")->text(), QStringLiteral("10")); // the current range
    EXPECT_EQ(field("fixedMaximum")->text(), QStringLiteral("20"));
    EXPECT_EQ(checkedTitle(m_control.rangeMenu()), QStringLiteral("Auto")); // until applied

    type("fixedMinimum", QStringLiteral("5"));
    type("fixedMaximum", QStringLiteral("5"));
    EXPECT_FALSE(applyButton()->isEnabled());
    type("fixedMaximum", QStringLiteral("3"));
    EXPECT_FALSE(applyButton()->isEnabled());
    type("fixedMaximum", QStringLiteral(""));
    EXPECT_FALSE(applyButton()->isEnabled());
    type("fixedMinimum", QStringLiteral("-0.5"));
    type("fixedMaximum", QStringLiteral("1e3"));
    EXPECT_TRUE(applyButton()->isEnabled());
    EXPECT_TRUE(m_emitted.empty());

    applyButton()->click();

    ASSERT_EQ(m_emitted.size(), 1u);
    EXPECT_EQ(m_emitted.back(), (DisplaySettings{.mode = RangeMode::Fixed, .range = {-0.5, 1000}}));
    EXPECT_FALSE(popup()->isVisible());
    EXPECT_EQ(button("rangeButton")->text(), QStringLiteral("Range: Fixed ▾"));
    EXPECT_EQ(checkedTitle(m_control.rangeMenu()), QStringLiteral("Fixed…"));
}

TEST_F(DisplaySettingsControlTest, EscapeLeavesTheRangeAsItWas) {
    openFixed();

    QTest::keyClick(field("fixedMinimum"), Qt::Key_Escape);

    EXPECT_FALSE(popup()->isVisible());
    EXPECT_TRUE(m_emitted.empty());
    EXPECT_EQ(checkedTitle(m_control.rangeMenu()), QStringLiteral("Auto"));
}

TEST_F(DisplaySettingsControlTest, ColoursOnlyForSingleChannel) {
    actionTitled(m_control.coloursMenu(), QStringLiteral("Turbo"))->trigger();
    ASSERT_EQ(m_emitted.size(), 1u);
    EXPECT_EQ(m_emitted.back(), DisplaySettings{.colours = ColourMap::Turbo});
    EXPECT_EQ(button("coloursButton")->text(), QStringLiteral("Colours: Turbo ▾"));

    m_control.setState(DisplaySettings{}, false);

    EXPECT_TRUE(button("coloursButton")->isHidden());
    for (const QAction* action : m_control.coloursMenu()->actions()) {
        EXPECT_FALSE(action->isEnabled());
    }
    m_control.setState(DisplaySettings{.colours = ColourMap::Viridis}, true);
    EXPECT_FALSE(button("coloursButton")->isHidden());
    EXPECT_EQ(button("coloursButton")->text(), QStringLiteral("Colours: Viridis ▾"));
    EXPECT_EQ(checkedTitle(m_control.coloursMenu()), QStringLiteral("Viridis"));
    EXPECT_EQ(m_emitted.size(), 1u); // setState is quiet
}

// New frames change what Auto would show, but never what is being typed.
TEST_F(DisplaySettingsControlTest, FixedFieldsSurviveNewFrames) {
    openFixed();
    type("fixedMinimum", QStringLiteral("5"));

    m_autoRange = {0, 200};
    QTest::qWait(10);

    EXPECT_EQ(field("fixedMinimum")->text(), QStringLiteral("5"));
    EXPECT_EQ(field("fixedMaximum")->text(), QStringLiteral("20"));
    EXPECT_TRUE(popup()->isVisible());
}

} // namespace
