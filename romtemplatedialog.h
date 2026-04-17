#ifndef ROMTEMPLATEDIALOG_H
#define ROMTEMPLATEDIALOG_H

#include <QDialog>
#include <QLabel>

class RomBitTemplate;

namespace Ui {
class RomTemplateDialog;
}

class RomTemplateDialog : public QDialog {
    Q_OBJECT
public:
    explicit RomTemplateDialog(QWidget *parent = nullptr);
    ~RomTemplateDialog();

    void setTemplate(RomBitTemplate *tmpl);
    void loadSettings(int cropW, int cropH, int searchRadius, double nccThreshold, bool alignEnabled = true);

signals:
    void buildRequested();
    void runTemplateDRCRequested();
    void correctionsRequested(double confidence);
    void alignEnabledChanged(bool enabled);
    void settingsChanged();  // emitted when any build-affecting parameter changes

private slots:
    void on_buildButton_clicked();
    void on_runTemplateDRCButton_clicked();
    void on_applyCorrectionsButton_clicked();
    void on_nccThresholdSpinBox_valueChanged(double value);
    void on_searchRadiusSpinBox_valueChanged(int value);
    void on_templateWSpinBox_valueChanged(int value);
    void on_templateHSpinBox_valueChanged(int value);
    void on_alignCheckBox_toggled(bool checked);

private:
    Ui::RomTemplateDialog *ui;
    QLabel *keyLabels[8];
};

#endif // ROMTEMPLATEDIALOG_H
