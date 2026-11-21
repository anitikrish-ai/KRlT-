#include "kconfig.h"
#include "kconfiggroup.h"
#include "ksharedconfig.h"
#include "klocalizedstring.h"
#include "kpluginfactory.h"
#include <QDir>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

KConfig::KConfig(const QString &file, OpenFlags mode, QStandardPaths::StandardLocation type)
    : m_fileName(file.isEmpty() ? QStringLiteral("kritarc") : file)
    , m_mode(mode)
{
    QString baseDir = QStandardPaths::writableLocation(type);
    if (baseDir.isEmpty()) {
        baseDir = QStringLiteral("/krita-data/config");
    }
    QDir().mkpath(baseDir);
    const QString fullPath = QDir(baseDir).filePath(m_fileName);
    m_settings = std::make_unique<QSettings>(fullPath, QSettings::IniFormat);
}

KConfig::~KConfig()
{
    sync();
}

KConfigGroup KConfig::group(const QString &group)
{
    return KConfigGroup(this, group);
}

const KConfigGroup KConfig::group(const QString &group) const
{
    return KConfigGroup(this, group);
}

bool KConfig::hasGroup(const QString &group) const
{
    return m_settings && m_settings->childGroups().contains(group);
}

void KConfig::deleteGroup(const QString &group, WriteConfigFlags flags)
{
    Q_UNUSED(flags);
    if (!m_settings) return;
    m_settings->beginGroup(group);
    m_settings->remove(QString());
    m_settings->endGroup();
}

QStringList KConfig::groupList() const
{
    return m_settings ? m_settings->childGroups() : QStringList();
}

bool KConfig::sync()
{
    if (m_settings) {
        m_settings->sync();
        return m_settings->status() == QSettings::NoError;
    }
    return true;
}

KConfigGroup::KConfigGroup() = default;

KConfigGroup::KConfigGroup(KConfigBase *master, const QString &group)
    : m_config(dynamic_cast<const KConfig *>(master))
    , m_groupName(group)
{}

KConfigGroup::KConfigGroup(const KConfigBase *master, const QString &group)
    : m_config(dynamic_cast<const KConfig *>(master))
    , m_groupName(group)
{}

KConfigGroup::KConfigGroup(const QExplicitlySharedDataPointer<KSharedConfig> &master, const QString &group)
    : m_config(master.data())
    , m_groupName(group)
{}

QString KConfigGroup::prefixedKey(const QString &key) const
{
    if (m_groupName.isEmpty()) return key;
    return m_groupName + QLatin1Char('/') + key;
}

bool KConfigGroup::exists() const
{
    return m_config && (m_groupName.isEmpty() || m_config->hasGroup(m_groupName));
}

bool KConfigGroup::sync()
{
    return m_config && m_config->settings() ? (m_config->settings()->sync(), true) : true;
}

bool KConfigGroup::hasKey(const QString &key) const
{
    return m_config && m_config->settings() && m_config->settings()->contains(prefixedKey(key));
}

void KConfigGroup::deleteEntry(const QString &key, WriteConfigFlags flags)
{
    Q_UNUSED(flags);
    if (m_config && m_config->settings()) {
        m_config->settings()->remove(prefixedKey(key));
    }
}

void KConfigGroup::deleteGroup(WriteConfigFlags flags)
{
    Q_UNUSED(flags);
    if (m_config && m_config->settings()) {
        m_config->settings()->beginGroup(m_groupName);
        m_config->settings()->remove(QString());
        m_config->settings()->endGroup();
    }
}

QStringList KConfigGroup::keyList() const
{
    if (!m_config || !m_config->settings()) return {};
    m_config->settings()->beginGroup(m_groupName);
    const QStringList keys = m_config->settings()->childKeys();
    m_config->settings()->endGroup();
    return keys;
}

QStringList KConfigGroup::groupList() const
{
    if (!m_config || !m_config->settings()) return {};
    m_config->settings()->beginGroup(m_groupName);
    const QStringList groups = m_config->settings()->childGroups();
    m_config->settings()->endGroup();
    return groups;
}

QMap<QString, QString> KConfigGroup::entryMap() const
{
    QMap<QString, QString> map;
    if (!m_config || !m_config->settings()) return map;
    m_config->settings()->beginGroup(m_groupName);
    for (const QString &k : m_config->settings()->childKeys()) {
        map.insert(k, m_config->settings()->value(k).toString());
    }
    m_config->settings()->endGroup();
    return map;
}

KConfigGroup KConfigGroup::group(const QString &subGroup)
{
    const QString joined = m_groupName.isEmpty() ? subGroup : (m_groupName + QLatin1Char('/') + subGroup);
    return KConfigGroup(m_config, joined);
}

const KConfigGroup KConfigGroup::group(const QString &subGroup) const
{
    const QString joined = m_groupName.isEmpty() ? subGroup : (m_groupName + QLatin1Char('/') + subGroup);
    return KConfigGroup(m_config, joined);
}

QString KConfigGroup::readEntry(const QString &key, const QString &aDefault) const
{
    if (!hasKey(key)) return aDefault;
    return readVariant(key, aDefault).toString();
}

QVariant KConfigGroup::readEntry(const QString &key, const QVariant &aDefault) const
{
    return readVariant(key, aDefault);
}

QStringList KConfigGroup::readEntry(const QString &key, const QStringList &aDefault) const
{
    if (!hasKey(key)) return aDefault;
    return readVariant(key, aDefault).toStringList();
}

QVariant KConfigGroup::readVariant(const QString &key, const QVariant &aDefault) const
{
    if (!m_config || !m_config->settings()) return aDefault;
    return m_config->settings()->value(prefixedKey(key), aDefault);
}

void KConfigGroup::writeVariant(const QString &key, const QVariant &value)
{
    if (!m_config || !m_config->settings()) return;
    m_config->settings()->setValue(prefixedKey(key), value);
}

KSharedConfig::Ptr KSharedConfig::openConfig(const QString &fileName,
                                             OpenFlags mode,
                                             QStandardPaths::StandardLocation type)
{
    static QMutex s_mutex;
    static QHash<QString, KSharedConfig::Ptr> s_cache;
    QMutexLocker lock(&s_mutex);
    const QString key = (fileName.isEmpty() ? QStringLiteral("kritarc") : fileName) + QStringLiteral("@") + QString::number(int(type));
    auto it = s_cache.find(key);
    if (it != s_cache.end()) {
        return it.value();
    }
    KSharedConfig::Ptr ptr(new KSharedConfig(fileName, mode, type));
    s_cache.insert(key, ptr);
    return ptr;
}

KLocalizedString KLocalizedString::substituteNext(const QString &val) const
{
    // Replace lowest numbered placeholder %1, %2, ...
    static const QRegularExpression re(QStringLiteral("%(\\d+)"));
    int lowest = 9999;
    auto it = re.globalMatch(m_text);
    while (it.hasNext()) {
        int n = it.next().captured(1).toInt();
        if (n < lowest) lowest = n;
    }
    QString updated = m_text;
    QString updatedPlural = m_pluralText;
    if (lowest < 9999) {
        const QString token = QLatin1Char('%') + QString::number(lowest);
        updated.replace(token, val);
        updatedPlural.replace(token, val);
    }
    return KLocalizedString(updated, m_pluralCount, updatedPlural);
}
