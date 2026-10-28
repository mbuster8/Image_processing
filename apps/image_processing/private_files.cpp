#include "private_files.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>

bool writeFileAtomically(const QString& path, const std::function<bool(QIODevice&)>& write, QString* error) {
    QSaveFile file(path);
    const auto fail = [error](const QString& reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(file.errorString());
    }
    if (!write(file)) {
        file.cancelWriting();
        return fail(file.errorString());
    }
    // QSaveFile notices a failed write and refuses to commit it.
    if (!file.commit()) {
        return fail(file.errorString());
    }
    if (!QFile::setPermissions(path, kOwnerOnlyFile)) {
        return fail(QCoreApplication::translate("PrivateFiles", "cannot make [%1] private").arg(path));
    }
    return true;
}
