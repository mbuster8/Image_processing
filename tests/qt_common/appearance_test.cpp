#include "appearance.h"

#include <QAction>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QSettings>
#include <QStringList>

#include <gtest/gtest.h>

namespace {

const QString kAppearanceKey = QStringLiteral("appearance");

// The viewer's settings, where the choice lives whichever program makes it. Spelled out, not taken from the
// code under test, so the test pins the place both programs read.
void setSharedValue(const QString& value) {
    QSettings(QSettings::defaultFormat(), QSettings::UserScope, QStringLiteral("proximasus.com"),
              QStringLiteral("Visual Debug Viewer"))
        .setValue(kAppearanceKey, value);
}

QString sharedValue() {
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope, QStringLiteral("proximasus.com"),
                     QStringLiteral("Visual Debug Viewer"))
        .value(kAppearanceKey)
        .toString();
}

QStringList texts(const QMenu& menu) {
    QStringList result;
    for (const QAction* action : menu.actions()) {
        result << action->text();
    }
    return result;
}

QStringList checkedTexts(const QMenu& menu) {
    QStringList result;
    for (const QAction* action : menu.actions()) {
        if (action->isChecked()) {
            result << action->text();
        }
    }
    return result;
}

QAction* actionNamed(const QMenu& menu, const QString& text) {
    for (QAction* action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

// The offscreen platform ignores colour schemes, but later tests should still find the default.
class Appearance : public testing::Test {
protected:
    void TearDown() override { qt_common::applyAppearance(qt_common::Appearance::System); }
};

TEST_F(Appearance, DefaultsToSystem) {
    EXPECT_EQ(qt_common::storedAppearance(), qt_common::Appearance::System);
}

TEST_F(Appearance, StoresAndReadsBack) {
    qt_common::storeAppearance(qt_common::Appearance::Light);
    EXPECT_EQ(qt_common::storedAppearance(), qt_common::Appearance::Light);
    EXPECT_EQ(sharedValue(), QStringLiteral("light"));
    qt_common::storeAppearance(qt_common::Appearance::Dark);
    EXPECT_EQ(qt_common::storedAppearance(), qt_common::Appearance::Dark);
    EXPECT_EQ(sharedValue(), QStringLiteral("dark"));
}

TEST_F(Appearance, UnrecognisedValueReadsAsSystem) {
    setSharedValue(QStringLiteral("purple"));
    EXPECT_EQ(qt_common::storedAppearance(), qt_common::Appearance::System);
}

TEST_F(Appearance, SharedBetweenApps) {
    const QString name = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("Visual Debug Example Sender"));
    qt_common::storeAppearance(qt_common::Appearance::Dark);
    QCoreApplication::setApplicationName(name);

    EXPECT_EQ(sharedValue(), QStringLiteral("dark"));
}

TEST_F(Appearance, MenuTicksStoredChoiceAndStoresSelection) {
    QMenu viewMenu;
    qt_common::storeAppearance(qt_common::Appearance::Dark);
    QMenu* menu = qt_common::addAppearanceMenu(&viewMenu);
    if (!qt_common::canChooseAppearance()) {
        EXPECT_EQ(menu, nullptr);
        EXPECT_TRUE(viewMenu.isEmpty());
        return;
    }
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menu->title(), QStringLiteral("Appearance"));
    EXPECT_EQ(texts(*menu),
              (QStringList{QStringLiteral("System"), QStringLiteral("Light"), QStringLiteral("Dark")}));
    EXPECT_EQ(checkedTexts(*menu), QStringList{QStringLiteral("Dark")});

    actionNamed(*menu, QStringLiteral("Light"))->trigger();

    EXPECT_EQ(qt_common::storedAppearance(), qt_common::Appearance::Light);
    EXPECT_EQ(checkedTexts(*menu), QStringList{QStringLiteral("Light")});
}

// The example sender and the viewer share the choice: whichever was not used to make it catches up when
// activated.
TEST_F(Appearance, MenuFollowsAnotherAppsChoiceOnActivation) {
    if (!qt_common::canChooseAppearance()) {
        GTEST_SKIP() << "View > Appearance needs Qt 6.8";
    }
    QMenu viewMenu;
    QMenu* menu = qt_common::addAppearanceMenu(&viewMenu);
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(checkedTexts(*menu), QStringList{QStringLiteral("System")});

    setSharedValue(QStringLiteral("dark"));
    emit qApp->applicationStateChanged(Qt::ApplicationActive);

    EXPECT_EQ(checkedTexts(*menu), QStringList{QStringLiteral("Dark")});
}

// Below other entries, a separator sets the submenu apart.
TEST_F(Appearance, FollowsExistingEntriesAfterASeparator) {
    if (!qt_common::canChooseAppearance()) {
        GTEST_SKIP() << "View > Appearance needs Qt 6.8";
    }
    QMenu viewMenu;
    viewMenu.addAction(QStringLiteral("Show Log"));
    QMenu* menu = qt_common::addAppearanceMenu(&viewMenu);

    ASSERT_EQ(viewMenu.actions().size(), 3);
    EXPECT_TRUE(viewMenu.actions().at(1)->isSeparator());
    EXPECT_EQ(viewMenu.actions().at(2)->menu(), menu);
}

} // namespace
