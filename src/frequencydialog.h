#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

class FrequencyInputDialog : public QDialog {
public:
    FrequencyInputDialog(const QVector<QPair<int, double>>& frequencies, QWidget* parent = nullptr) 
        : QDialog(parent) 
    {
        setWindowTitle("Frequenz auswählen");
        
        QVBoxLayout* layout = new QVBoxLayout(this);
        
        // Frequenzliste anzeigen
        QLabel* freqLabel = new QLabel(this);
        freqLabel->setTextFormat(Qt::RichText);
        freqLabel->setText(formatFrequencies(frequencies));
        layout->addWidget(freqLabel);
        
        // Spinbox für Eingabe
        QSpinBox* spinBox = new QSpinBox(this);
        spinBox->setRange(1, frequencies.size());
        layout->addWidget(spinBox);
        
        // Standard OK/Cancel Buttons
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttonBox);
        
        m_spinBox = spinBox;
    }
    
    int getSelectedNumber() const {
        return m_spinBox->value();
    }
    
private:
QString formatFrequencies(const QVector<QPair<int, double>>& frequencies) {
    QString text = "<html><body>";
    text += "<table><tr>";
    
    const int itemsPerColumn = 20;
    const int numColumns = 3;
    const QString columnSpacing = "&nbsp;&nbsp;&nbsp;&nbsp;"; // Abstand zwischen Spalten
    
    for (int col = 0; col < numColumns; ++col) {
        if (col > 0) {
            text += QString("<td>%1</td>").arg(columnSpacing);
        }
        
        text += "<td><pre>";  // <pre> für monospace-formatierung
        
        int startIdx = col * itemsPerColumn;
        int endIdx = qMin(startIdx + itemsPerColumn, frequencies.size());
        
        for (int i = startIdx; i < endIdx; ++i) {
            const auto& pair = frequencies[i];
            QString line = QString("%1: %2\n")
                            .arg(pair.first, 3)
                            .arg(pair.second, 8, 'f', 2);
            if (pair.second < 0) {
                line = QString("<span style='color: red'>%1</span>").arg(line);
            }
            text += line;
        }
        
        text += "</pre></td>";
    }
    
    text += "</tr></table></body></html>";
    return text;
}
    
    QSpinBox* m_spinBox;
};

