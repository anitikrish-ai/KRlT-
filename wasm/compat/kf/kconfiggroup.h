#ifndef KRITA_WASM_KCONFIGGROUP_H
#define KRITA_WASM_KCONFIGGROUP_H

#include "kconfig.h"
#include <QColor>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariant>

class KSharedConfig;

class KConfigGroup : public KConfigBase
{
public:
    KConfigGroup();
    KConfigGroup(KConfigBase *master, const QString &group);
    KConfigGroup(const KConfigBase *master, const QString &group);
    KConfigGroup(const QExplicitlySharedDataPointer<KSharedConfig> &master, const QString &group);
    KConfigGroup(const KConfigGroup &other) = default;
    KConfigGroup &operator=(const KConfigGroup &other) = default;
    ~KConfigGroup() override = default;

    bool isValid() const { return m_config != nullptr; }
    QString name() const { return m_groupName; }
    bool exists() const;
    bool sync() override;

    bool hasKey(const QString &key) const;
    bool hasKey(const char *key) const { return hasKey(QString::fromUtf8(key)); }

    void deleteEntry(const QString &key, WriteConfigFlags flags = Normal);
    void deleteEntry(const char *key, WriteConfigFlags flags = Normal) { deleteEntry(QString::fromUtf8(key), flags); }
    void deleteGroup(WriteConfigFlags flags = Normal);

    QStringList keyList() const;
    QStringList groupList() const;
    QMap<QString, QString> entryMap() const;

    KConfigGroup group(const QString &subGroup);
    const KConfigGroup group(const QString &subGroup) const;

    template <typename T>
    T readEntry(const QString &key, const T &aDefault) const;

    template <typename T>
    T readEntry(const char *key, const T &aDefault) const {
        return readEntry<T>(QString::fromUtf8(key), aDefault);
    }

    QString readEntry(const QString &key, const QString &aDefault = QString()) const;
    QString readEntry(const QString &key, const char *aDefault) const {
        return readEntry(key, QString::fromUtf8(aDefault));
    }
    QString readEntry(const char *key, const char *aDefault) const {
        return readEntry(QString::fromUtf8(key), QString::fromUtf8(aDefault));
    }
    QString readEntry(const char *key, const QString &aDefault = QString()) const {
        return readEntry(QString::fromUtf8(key), aDefault);
    }
    QVariant readEntry(const QString &key, const QVariant &aDefault) const;
    QVariant readEntry(const char *key, const QVariant &aDefault) const {
        return readEntry(QString::fromUtf8(key), aDefault);
    }
    QStringList readEntry(const QString &key, const QStringList &aDefault) const;
    QStringList readEntry(const char *key, const QStringList &aDefault) const {
        return readEntry(QString::fromUtf8(key), aDefault);
    }

    template <typename T>
    QList<T> readEntry(const QString &key, const QList<T> &aDefault) const {
        if (!hasKey(key)) return aDefault;
        const QVariant v = readVariant(key, QVariant::fromValue(aDefault));
        if (v.canConvert<QList<T>>()) return v.value<QList<T>>();
        return aDefault;
    }

    template <typename T>
    void writeEntry(const QString &key, const T &value, WriteConfigFlags flags = Normal) {
        Q_UNUSED(flags);
        writeVariant(key, QVariant::fromValue(value));
    }
    template <typename T>
    void writeEntry(const char *key, const T &value, WriteConfigFlags flags = Normal) {
        writeEntry<T>(QString::fromUtf8(key), value, flags);
    }
    void writeEntry(const QString &key, const QVariant &value, WriteConfigFlags flags = Normal) {
        Q_UNUSED(flags);
        writeVariant(key, value);
    }
    void writeEntry(const char *key, const QVariant &value, WriteConfigFlags flags = Normal) {
        writeEntry(QString::fromUtf8(key), value, flags);
    }
    void writeEntry(const QString &key, const QStringList &value, WriteConfigFlags flags = Normal) {
        Q_UNUSED(flags);
        writeVariant(key, QVariant(value));
    }
    void writeEntry(const char *key, const char *value, WriteConfigFlags flags = Normal) {
        writeEntry(QString::fromUtf8(key), QString::fromUtf8(value), flags);
    }

    QVariant readVariant(const QString &key, const QVariant &aDefault = QVariant()) const;
    void writeVariant(const QString &key, const QVariant &value);

private:
    QString prefixedKey(const QString &key) const;
    const KConfig *m_config {nullptr};
    QString m_groupName;
};

template <typename T>
inline T KConfigGroup::readEntry(const QString &key, const T &aDefault) const {
    if (!hasKey(key)) return aDefault;
    const QVariant v = readVariant(key, QVariant::fromValue(aDefault));
    if (v.canConvert<T>()) return v.value<T>();
    return aDefault;
}

template <>
inline bool KConfigGroup::readEntry<bool>(const QString &key, const bool &aDefault) const {
    if (!hasKey(key)) return aDefault;
    return readVariant(key, aDefault).toBool();
}

template <>
inline int KConfigGroup::readEntry<int>(const QString &key, const int &aDefault) const {
    if (!hasKey(key)) return aDefault;
    bool ok = false;
    int val = readVariant(key, aDefault).toInt(&ok);
    return ok ? val : aDefault;
}

template <>
inline double KConfigGroup::readEntry<double>(const QString &key, const double &aDefault) const {
    if (!hasKey(key)) return aDefault;
    bool ok = false;
    double val = readVariant(key, aDefault).toDouble(&ok);
    return ok ? val : aDefault;
}

template <>
inline float KConfigGroup::readEntry<float>(const QString &key, const float &aDefault) const {
    if (!hasKey(key)) return aDefault;
    bool ok = false;
    float val = readVariant(key, aDefault).toFloat(&ok);
    return ok ? val : aDefault;
}

template <>
inline QString KConfigGroup::readEntry<QString>(const QString &key, const QString &aDefault) const {
    return readEntry(key, aDefault);
}

template <>
inline QColor KConfigGroup::readEntry<QColor>(const QString &key, const QColor &aDefault) const {
    if (!hasKey(key)) return aDefault;
    QVariant v = readVariant(key, aDefault);
    if (v.canConvert<QColor>()) return v.value<QColor>();
    QColor c(v.toString());
    return c.isValid() ? c : aDefault;
}

#endif // KRITA_WASM_KCONFIGGROUP_H
