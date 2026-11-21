#ifndef KRITA_WASM_KACTIONCOLLECTION_H
#define KRITA_WASM_KACTIONCOLLECTION_H

#include <QAction>
#include <QObject>
#include <QString>

class KisKActionCollection : public QObject
{
    Q_OBJECT
public:
    explicit KisKActionCollection(QObject *parent = nullptr) : QObject(parent) {}
    QAction *addAction(const QString &name, QAction *action) {
        if (action) {
            action->setObjectName(name);
            action->setParent(this);
        }
        return action;
    }
};

#endif // KRITA_WASM_KACTIONCOLLECTION_H
