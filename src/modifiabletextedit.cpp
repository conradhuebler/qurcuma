#include "modifiabletextedit.h"
#include <QKeyEvent>

// Claude Generated - Phase 2.3: Text editor with modification tracking

ModifiableTextEdit::ModifiableTextEdit(QWidget *parent)
    : QTextEdit(parent), m_modified(false)
{
    // Connect document changes to track modifications
    connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        if (!m_modified) {
            m_modified = true;
            emit modificationChanged(true);
        }
    });
}

void ModifiableTextEdit::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit modificationChanged(modified);
    }
}

void ModifiableTextEdit::resetModified()
{
    m_modified = false;
    document()->setModified(false);
    emit modificationChanged(false);
}

void ModifiableTextEdit::keyPressEvent(QKeyEvent *event)
{
    // Mark as modified on any key press (unless it's just navigation)
    if (!m_modified && event->text().length() > 0 && !event->text()[0].isNull()) {
        m_modified = true;
        emit modificationChanged(true);
    }
    QTextEdit::keyPressEvent(event);
}
