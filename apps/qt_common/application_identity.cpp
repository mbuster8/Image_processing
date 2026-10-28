#include "application_identity.h"

#include <QCoreApplication>

namespace qt_common {

void setApplicationIdentity(const QString& applicationName) {
    QCoreApplication::setOrganizationDomain(QLatin1String(kOrganisationDomain));
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(applicationName);
}

} // namespace qt_common
