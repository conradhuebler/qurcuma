#ifndef MODIFIABLETEXTEDIT_H
#define MODIFIABLETEXTEDIT_H

#include <QTextEdit>

// Claude Generated - Phase 2.3: Text editor with modification tracking
class ModifiableTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit ModifiableTextEdit(QWidget *parent = nullptr);

    bool isModified() const { return m_modified; }
    void setModified(bool modified);
    void resetModified();

signals:
    void modificationChanged(bool modified);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_modified = false;
};

#endif // MODIFIABLETEXTEDIT_H
