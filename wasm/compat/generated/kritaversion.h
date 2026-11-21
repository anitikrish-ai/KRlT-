#ifndef _KRITA_VERSION_H_
#define _KRITA_VERSION_H_

#include "kritaversion_export.h"

#define KRITA_VERSION_STRING "6.1.0-wasm"
#define KRITA_STABLE_VERSION_MAJOR 6
#define KRITA_STABLE_VERSION_MINOR 1
#define KRITA_VERSION_RELEASE 0
#define KRITA_ALPHA 1

#define KRITA_MAKE_VERSION(a,b,c) (((a) << 16) | ((b) << 8) | (c))
#define KRITA_VERSION KRITA_MAKE_VERSION(KRITA_STABLE_VERSION_MAJOR, KRITA_STABLE_VERSION_MINOR, KRITA_VERSION_RELEASE)
#define KRITA_VERSION_CHECK(a, b, c) (KRITA_VERSION >= KRITA_MAKE_VERSION(a, b, c))

inline unsigned int KritaVersion() { return KRITA_VERSION; }
inline unsigned int KritaVersionMajor() { return KRITA_STABLE_VERSION_MAJOR; }
inline unsigned int KritaVersionMinor() { return KRITA_STABLE_VERSION_MINOR; }
inline unsigned int KritaVersionRelease() { return KRITA_VERSION_RELEASE; }
inline const char *KritaVersionString() { return KRITA_VERSION_STRING; }

#endif // _KRITA_VERSION_H_
