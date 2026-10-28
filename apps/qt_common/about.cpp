#include "about.h"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QtGlobal>

#include <opencv2/core/utility.hpp>

namespace qt_common {

QString aboutText(const QString& description) {
    return QCoreApplication::translate("About",
                                       "<p><b>%1</b> %2</p><p>%3</p><p>Built with OpenCV %4 and Qt %5.</p>")
        .arg(QCoreApplication::applicationName().toHtmlEscaped(),
             QCoreApplication::applicationVersion().toHtmlEscaped(), description.toHtmlEscaped(),
             QString::fromStdString(cv::getVersionString()), QLatin1String(qVersion()));
}

QMenu* addHelpMenu(QMenuBar* menuBar, const QString& description) {
    QMenu* help = menuBar->addMenu(QCoreApplication::translate("About", "&Help"));
    help->setObjectName(QStringLiteral("menuHelp"));
    const QString title =
        QCoreApplication::translate("About", "About %1").arg(QCoreApplication::applicationName());
    QAction* about = help->addAction(title);
    about->setMenuRole(QAction::AboutRole);
    QObject::connect(about, &QAction::triggered, menuBar, [menuBar, title, description] {
        QMessageBox::about(menuBar->window(), title, aboutText(description));
    });
    return help;
}

} // namespace qt_common
