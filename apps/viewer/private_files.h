#pragma once

#include <QFileDevice>
#include <QIODevice>
#include <QString>

#include <functional>

// Images may be private, and the log names them: everything the viewer keeps is readable by its owner only.
inline constexpr QFileDevice::Permissions kOwnerOnlyFile = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
inline constexpr QFileDevice::Permissions kOwnerOnlyFolder = kOwnerOnlyFile | QFileDevice::ExeOwner;

// Replaces the file at `path` all at once, through QSaveFile, with what `write` puts into the device, and
// makes it owner-only. A crash or a failed write leaves the previous file as it was. `write` returns false to
// give up. False on failure, with the reason in `error` when it is given.
bool writeFileAtomically(const QString& path, const std::function<bool(QIODevice&)>& write,
                         QString* error = nullptr);
