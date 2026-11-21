#ifndef KRITA_WASM_KSHAREDCONFIG_H
#define KRITA_WASM_KSHAREDCONFIG_H

#include "kconfig.h"
#include "kconfiggroup.h"
#include <QExplicitlySharedDataPointer>
#include <QSharedData>

class KSharedConfig : public KConfig, public QSharedData
{
public:
    using Ptr = QExplicitlySharedDataPointer<KSharedConfig>;

    static Ptr openConfig(const QString &fileName = QString(),
                          OpenFlags mode = FullConfig,
                          QStandardPaths::StandardLocation type = QStandardPaths::GenericConfigLocation);

private:
    KSharedConfig(const QString &file, OpenFlags mode, QStandardPaths::StandardLocation type)
        : KConfig(file, mode, type) {}
};

#endif // KRITA_WASM_KSHAREDCONFIG_H
