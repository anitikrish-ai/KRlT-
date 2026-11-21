#ifndef KRITA_WASM_KSTANDARDACTION_H
#define KRITA_WASM_KSTANDARDACTION_H

#include <QString>

namespace KStandardAction {
enum StandardAction {
    Undo,
    Redo
};

inline QString name(StandardAction id) {
    switch (id) {
    case Undo: return QStringLiteral("edit_undo");
    case Redo: return QStringLiteral("edit_redo");
    }
    return QString();
}
}

#include <QKeySequence>
#include <QList>

namespace KStandardShortcut {
inline QList<QKeySequence> undo() { return QKeySequence::keyBindings(QKeySequence::Undo); }
inline QList<QKeySequence> redo() { return QKeySequence::keyBindings(QKeySequence::Redo); }
}

#endif // KRITA_WASM_KSTANDARDACTION_H
