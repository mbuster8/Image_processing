#include "appearance.h"

#include "application_identity.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QSettings>
#include <QStyleHints>
#include <QtGlobal>

#include <array>
#include <memory>

namespace qt_common {

namespace {

const QString kAppearanceKey = QStringLiteral("appearance");

struct Choice {
    Appearance appearance;
    const char* value; // as stored
    const char* title; // in the menu
};

constexpr std::array kChoices{Choice{Appearance::System, "system", QT_TRANSLATE_NOOP("Appearance", "System")},
                              Choice{Appearance::Light, "light", QT_TRANSLATE_NOOP("Appearance", "Light")},
                              Choice{Appearance::Dark, "dark", QT_TRANSLATE_NOOP("Appearance", "Dark")}};

// The viewer's settings, whichever program runs: QSettings() would name the client's own.
std::unique_ptr<QSettings> sharedSettings() {
    return std::make_unique<QSettings>(QSettings::defaultFormat(), QSettings::UserScope,
                                       QLatin1String(kOrganisationDomain), QLatin1String(kViewerName));
}

} // namespace

bool canChooseAppearance() { return QT_VERSION >= QT_VERSION_CHECK(6, 8, 0); }

Appearance storedAppearance() {
    const QString value = sharedSettings()->value(kAppearanceKey).toString();
    for (const Choice& choice : kChoices) {
        if (value == QLatin1String(choice.value)) {
            return choice.appearance;
        }
    }
    return Appearance::System;
}

void storeAppearance(Appearance appearance) {
    for (const Choice& choice : kChoices) {
        if (choice.appearance == appearance) {
            sharedSettings()->setValue(kAppearanceKey, QLatin1String(choice.value));
        }
    }
}

void applyAppearance(Appearance appearance) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QStyleHints* hints = QGuiApplication::styleHints();
    switch (appearance) {
    case Appearance::System:
        hints->unsetColorScheme();
        break;
    case Appearance::Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Appearance::Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    }
#else
    Q_UNUSED(appearance);
#endif
}

QMenu* addAppearanceMenu(QMenu* viewMenu) {
    if (!canChooseAppearance()) {
        return nullptr;
    }
    if (!viewMenu->isEmpty()) {
        viewMenu->addSeparator();
    }
    QMenu* menu = viewMenu->addMenu(QCoreApplication::translate("Appearance", "Appearance"));
    menu->setObjectName(QStringLiteral("appearanceMenu"));
    auto* group = new QActionGroup(menu);
    for (const Choice& choice : kChoices) {
        QAction* action = menu->addAction(QCoreApplication::translate("Appearance", choice.title));
        action->setCheckable(true);
        action->setData(static_cast<int>(choice.appearance));
        group->addAction(action);
    }
    const auto showStored = [group] {
        const int stored = static_cast<int>(storedAppearance());
        for (QAction* action : group->actions()) {
            action->setChecked(action->data().toInt() == stored);
        }
    };
    showStored();
    QObject::connect(group, &QActionGroup::triggered, menu, [](QAction* action) {
        const auto appearance = static_cast<Appearance>(action->data().toInt());
        storeAppearance(appearance);
        applyAppearance(appearance);
    });
    QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged, menu,
                     [showStored](Qt::ApplicationState state) {
                         if (state == Qt::ApplicationActive) {
                             applyAppearance(storedAppearance());
                             showStored();
                         }
                     });
    return menu;
}

} // namespace qt_common
