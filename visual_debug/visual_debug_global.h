#ifndef VISUAL_DEBUG_GLOBAL_H
#define VISUAL_DEBUG_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(VISUAL_DEBUG_LIBRARY)
#  define VISUAL_DEBUGSHARED_EXPORT Q_DECL_EXPORT
#else
#  define VISUAL_DEBUGSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // VISUAL_DEBUG_GLOBAL_H