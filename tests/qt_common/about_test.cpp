#include "about.h"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QMenuBar>
#include <QString>
#include <QtGlobal>

#include <gtest/gtest.h>
#include <opencv2/core/utility.hpp>

namespace {

TEST(About, TextNamesTheAppAndWhatItIsBuiltWith) {
    const QString text = qt_common::aboutText(QStringLiteral("Shows images."));

    EXPECT_TRUE(text.contains(QCoreApplication::applicationName())) << text.toStdString();
    EXPECT_TRUE(text.contains(QStringLiteral("Shows images."))) << text.toStdString();
    EXPECT_TRUE(text.contains(QStringLiteral("OpenCV ") + QString::fromStdString(cv::getVersionString())))
        << text.toStdString();
    EXPECT_TRUE(text.contains(QStringLiteral("Qt ") + QLatin1String(qVersion()))) << text.toStdString();
}

// The text is rich text, so a description is shown as written, never read as markup.
TEST(About, TextShowsTheDescriptionAsWritten) {
    const QString text = qt_common::aboutText(QStringLiteral("sendImage() <b>"));

    EXPECT_TRUE(text.contains(QStringLiteral("sendImage() &lt;b&gt;"))) << text.toStdString();
}

// macOS moves an action with the About role into the application menu; elsewhere it stays under Help.
TEST(About, HelpMenuHoldsAboutWithTheAboutRole) {
    QMenuBar menuBar;

    const QMenu* help = qt_common::addHelpMenu(&menuBar, QStringLiteral("Shows images."));

    ASSERT_NE(help, nullptr);
    EXPECT_EQ(help->objectName(), QStringLiteral("menuHelp"));
    EXPECT_EQ(help->title(), QStringLiteral("&Help"));
    ASSERT_EQ(help->actions().size(), 1);
    EXPECT_EQ(help->actions().front()->text(),
              QStringLiteral("About ") + QCoreApplication::applicationName());
    EXPECT_EQ(help->actions().front()->menuRole(), QAction::AboutRole);
}

} // namespace
