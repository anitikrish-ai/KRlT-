#ifndef KRITA_WASM_KCONFIG_H
#define KRITA_WASM_KCONFIG_H

#include <QExplicitlySharedDataPointer>
#include <QSharedData>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <memory>

class KConfigGroup;

class KConfigBase
{
public:
    enum WriteConfigFlag {
        Persistent = 0x01,
        Global = 0x02,
        Localized = 0x04,
        Normal = Persistent
    };
    Q_DECLARE_FLAGS(WriteConfigFlags, WriteConfigFlag)
    virtual ~KConfigBase() = default;
    virtual bool sync() = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KConfigBase::WriteConfigFlags)

class KConfig : public KConfigBase
{
public:
    enum OpenFlag {
        IncludeGlobals = 0x01,
        CascadeConfig = 0x02,
        SimpleConfig = 0x00,
        NoCascade = IncludeGlobals,
        NoGlobals = CascadeConfig,
        FullConfig = IncludeGlobals | CascadeConfig
    };
    Q_DECLARE_FLAGS(OpenFlags, OpenFlag)

    explicit KConfig(const QString &file = QString(),
                     OpenFlags mode = FullConfig,
                     QStandardPaths::StandardLocation type = QStandardPaths::GenericConfigLocation);
    ~KConfig() override;

    KConfigGroup group(const QString &group);
    const KConfigGroup group(const QString &group) const;
    bool hasGroup(const QString &group) const;
    void deleteGroup(const QString &group, WriteConfigFlags flags = Normal);
    QStringList groupList() const;
    QString name() const { return m_fileName; }
    bool sync() override;

    QSettings *settings() const { return m_settings.get(); }

protected:
    QString m_fileName;
    OpenFlags m_mode;
    std::unique_ptr<QSettings> m_settings;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KConfig::OpenFlags)

#endif // KRITA_WASM_KCONFIG_H
