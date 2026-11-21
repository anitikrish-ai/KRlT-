#ifndef KRITA_WASM_KLOCALIZEDSTRING_H
#define KRITA_WASM_KLOCALIZEDSTRING_H

#include <QLocale>
#include <QString>
#include <QStringList>
#include <QVariant>

class KLocalizedString
{
public:
    KLocalizedString() = default;
    explicit KLocalizedString(const QString &text, int pluralCount = -1, const QString &pluralText = QString())
        : m_text(text), m_pluralCount(pluralCount), m_pluralText(pluralText) {}

    bool isEmpty() const { return m_text.isEmpty(); }

    QString toString() const {
        if (m_pluralCount >= 0 && !m_pluralText.isEmpty() && m_pluralCount != 1) {
            return m_pluralText;
        }
        return m_text;
    }

    QString toString(const QStringList &languages) const {
        Q_UNUSED(languages);
        return toString();
    }

    template <typename T>
    KLocalizedString subs(const T &a, int fieldWidth = 0, int base = 10, QChar fillChar = QLatin1Char(' ')) const {
        Q_UNUSED(fieldWidth);
        Q_UNUSED(base);
        Q_UNUSED(fillChar);
        return substituteNext(QString::number(a));
    }

    KLocalizedString subs(const QString &a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const {
        Q_UNUSED(fieldWidth);
        Q_UNUSED(fillChar);
        return substituteNext(a);
    }

    KLocalizedString subs(const char *a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const {
        Q_UNUSED(fieldWidth);
        Q_UNUSED(fillChar);
        return substituteNext(QString::fromUtf8(a));
    }

    KLocalizedString subs(QChar a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const {
        Q_UNUSED(fieldWidth);
        Q_UNUSED(fillChar);
        return substituteNext(QString(a));
    }

    KLocalizedString subs(double a, int fieldWidth = 0, char format = 'g', int precision = -1, QChar fillChar = QLatin1Char(' ')) const {
        Q_UNUSED(fieldWidth);
        Q_UNUSED(fillChar);
        return substituteNext(QString::number(a, format, precision));
    }

    KLocalizedString subs(float a, int fieldWidth = 0, char format = 'g', int precision = -1, QChar fillChar = QLatin1Char(' ')) const {
        Q_UNUSED(fieldWidth);
        Q_UNUSED(fillChar);
        return substituteNext(QString::number(static_cast<double>(a), format, precision));
    }

    static QStringList languages() {
        return QLocale().uiLanguages();
    }

    static void setApplicationDomain(const char *domain) {
        Q_UNUSED(domain);
    }

    static QByteArray applicationDomain() {
        return QByteArray("krita");
    }

private:
    KLocalizedString substituteNext(const QString &val) const;
    QString m_text;
    int m_pluralCount {-1};
    QString m_pluralText;
};

inline KLocalizedString ki18n(const char *text) {
    return KLocalizedString(QString::fromUtf8(text));
}

inline KLocalizedString ki18nc(const char *context, const char *text) {
    Q_UNUSED(context);
    return KLocalizedString(QString::fromUtf8(text));
}

inline KLocalizedString ki18np(const char *singular, const char *plural) {
    return KLocalizedString(QString::fromUtf8(singular), 1, QString::fromUtf8(plural));
}

inline KLocalizedString ki18ncp(const char *context, const char *singular, const char *plural) {
    Q_UNUSED(context);
    return KLocalizedString(QString::fromUtf8(singular), 1, QString::fromUtf8(plural));
}

template <typename... Args>
inline QString i18n(const char *text, const Args &...args) {
    KLocalizedString k(QString::fromUtf8(text));
    ((k = k.subs(args)), ...);
    return k.toString();
}

template <typename... Args>
inline QString i18n(const QByteArray &text, const Args &...args) {
    return i18n(text.constData(), args...);
}

template <typename... Args>
inline QString i18nc(const char *context, const char *text, const Args &...args) {
    Q_UNUSED(context);
    return i18n(text, args...);
}

template <typename... Args>
inline QString i18nc(const char *context, const QByteArray &text, const Args &...args) {
    Q_UNUSED(context);
    return i18n(text.constData(), args...);
}

template <typename... Args>
inline QString i18np(const char *singular, const char *plural, int count, const Args &...args) {
    KLocalizedString k(QString::fromUtf8(singular), count, QString::fromUtf8(plural));
    k = k.subs(count);
    ((k = k.subs(args)), ...);
    return k.toString();
}

template <typename... Args>
inline QString i18ncp(const char *context, const char *singular, const char *plural, int count, const Args &...args) {
    Q_UNUSED(context);
    return i18np(singular, plural, count, args...);
}

#endif // KRITA_WASM_KLOCALIZEDSTRING_H
