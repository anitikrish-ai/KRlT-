#ifndef KRITA_WASM_KPLUGINFACTORY_H
#define KRITA_WASM_KPLUGINFACTORY_H

#include <QObject>
#include <QVariantList>
#include <QString>

class KPluginFactory : public QObject
{
    Q_OBJECT
public:
    explicit KPluginFactory(QObject *parent = nullptr) : QObject(parent) {}
    ~KPluginFactory() override = default;

    template <typename T>
    T *create(QObject *parent = nullptr, const QVariantList &args = QVariantList()) {
        QObject *obj = createObject(parent, args);
        return qobject_cast<T *>(obj);
    }

protected:
    virtual QObject *createObject(QObject *parent, const QVariantList &args) {
        Q_UNUSED(parent);
        Q_UNUSED(args);
        return nullptr;
    }
};

#define K_PLUGIN_FACTORY_WITH_JSON(name, jsonFile, pluginRegistrations) \
    class name : public KPluginFactory { \
        Q_OBJECT \
        Q_PLUGIN_METADATA(IID "org.kde.KPluginFactory" FILE jsonFile) \
    public: \
        explicit name(QObject *parent = nullptr) : KPluginFactory(parent) {} \
    };

#endif // KRITA_WASM_KPLUGINFACTORY_H
