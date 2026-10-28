#include "application_identity.h"

#include <QApplication>
#include <QByteArray>
#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

// Every test starts from the defaults, as on a first start, whether ctest runs it alone or the binary runs
// them all in one process: a sort or a width one test chose never reaches the next. The viewer's settings
// hold the appearance the client shares.
class ClearSettingsBeforeEachTest : public testing::EmptyTestEventListener {
    void OnTestStart(const testing::TestInfo& /*test*/) override {
        QSettings().clear();
        QSettings(QSettings::defaultFormat(), QSettings::UserScope,
                  QLatin1String(qt_common::kOrganisationDomain), QLatin1String(qt_common::kViewerName))
            .clear();
    }
};

} // namespace

// Widgets need a QApplication. The offscreen platform lets these tests run without a display, from ctest,
// Qt Creator or a CI runner alike. Settings go to INI files in a folder that lives for the whole run, so a
// test never reads or changes the owner's real viewer settings.
int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    testing::InitGoogleTest(&argc, argv);
    const QTemporaryDir settingsFolder;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsFolder.path());
    QApplication application(argc, argv);
    qt_common::setApplicationIdentity(QStringLiteral("Image Processing Tests"));
    testing::UnitTest::GetInstance()->listeners().Append(new ClearSettingsBeforeEachTest);
    return RUN_ALL_TESTS();
}
