#pragma once

#include <QString>

namespace qt_common {

inline constexpr char kOrganisationDomain[] = "proximasus.com";
// The viewer's application name, which also names the settings the viewer and the example sender share.
inline constexpr char kViewerName[] = "Visual Debug Viewer";
inline constexpr char kExampleSenderName[] = "Visual Debug Example Sender";

// Names the application for QSettings and QStandardPaths: the organisation domain proximasus.com, no
// organisation name, so settings live in com.proximasus.<name> and the data folder needs no company folder.
// Call it right after constructing the application object.
void setApplicationIdentity(const QString& applicationName);

} // namespace qt_common
