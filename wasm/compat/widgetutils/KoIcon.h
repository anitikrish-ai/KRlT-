#ifndef KOICON_H
#define KOICON_H

#include <QIcon>
#include <QLatin1String>
#include <QString>

namespace KisIconUtils {
inline QIcon loadIcon(const QString &name) {
    return QIcon::fromTheme(name);
}
}

#define koIcon(name) (KisIconUtils::loadIcon(QLatin1String(name)))
#define kisIcon(name) (KisIconUtils::loadIcon(name))
#define koIconName(name) (QLatin1String(name))
#define koIconNameCStr(name) (name)
#define koIconNeeded(comment, neededName) (KisIconUtils::loadIcon(QLatin1String(neededName)))
#define koIconNeededWithSubs(comment, neededName, substituteName) (KisIconUtils::loadIcon(QLatin1String(substituteName)))

#endif // KOICON_H
