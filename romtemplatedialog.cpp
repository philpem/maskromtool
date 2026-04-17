#include "romtemplatedialog.h"
#include "rombittemplate.h"
#include "romruletemplate.h"
#include "maskromtool_autogen/include/ui_romtemplatedialog.h"

#include <QLabel>
#include <QPixmap>

RomTemplateDialog::RomTemplateDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RomTemplateDialog) {
    ui->setupUi(this);
    keyLabels[0] = ui->keyLabel_0;
    keyLabels[1] = ui->keyLabel_1;
    keyLabels[2] = ui->keyLabel_2;
    keyLabels[3] = ui->keyLabel_3;
    keyLabels[4] = ui->keyLabel_4;
    keyLabels[5] = ui->keyLabel_5;
    keyLabels[6] = ui->keyLabel_6;
    keyLabels[7] = ui->keyLabel_7;
    for(int k = 0; k < 8; k++){
        keyLabels[k]->setScaledContents(true);
        keyLabels[k]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
}

RomTemplateDialog::~RomTemplateDialog() {
    delete ui;
}

void RomTemplateDialog::setTemplate(RomBitTemplate *tmpl) {
    if(!tmpl || !tmpl->isBuilt()) {
        ui->statusLabel->setText("No templates built.");
        for(int k = 0; k < 8; k++)
            keyLabels[k]->setText(QString::number(k));
        return;
    }

    // Reflect actual dimensions used — block signals so we don't override TEMPLATE_W/H.
    ui->templateWSpinBox->blockSignals(true);
    ui->templateHSpinBox->blockSignals(true);
    ui->templateWSpinBox->setValue(tmpl->templateW());
    ui->templateHSpinBox->setValue(tmpl->templateH());
    ui->templateWSpinBox->blockSignals(false);
    ui->templateHSpinBox->blockSignals(false);

    QStringList status;
    for(int k = 0; k < 8; k++) {
        int L = (k >> 2) & 1, C = (k >> 1) & 1, R = k & 1;
        QString tip = QString("Key %1: left=%2 center=%3 right=%4\n%5 training samples")
            .arg(k).arg(L).arg(C).arg(R).arg(tmpl->sampleCount(k));
        keyLabels[k]->setToolTip(tip);
        if(tmpl->hasTemplate(k)) {
            keyLabels[k]->setPixmap(QPixmap::fromImage(tmpl->templateImage(k)));
            keyLabels[k]->setText(QString());
        } else {
            keyLabels[k]->setPixmap(QPixmap());
            keyLabels[k]->setText(QString("(empty)"));
        }
        status << QString("k%1:%2").arg(k).arg(tmpl->sampleCount(k));
    }
    ui->statusLabel->setText(
        QString("Fixed bits: %1  Template: %2×%3px  Samples: %4")
            .arg(tmpl->fixedBitCount())
            .arg(tmpl->templateW()).arg(tmpl->templateH())
            .arg(status.join(" ")));
}

void RomTemplateDialog::loadSettings(int cropW, int cropH, int searchRadius, double nccThreshold, bool alignEnabled) {
    ui->templateWSpinBox->setValue(cropW);
    ui->templateHSpinBox->setValue(cropH);
    ui->searchRadiusSpinBox->setValue(searchRadius);
    ui->nccThresholdSpinBox->setValue(nccThreshold);
    RomBitTemplate::ALIGN_ENABLED = alignEnabled;
    ui->alignCheckBox->blockSignals(true);
    ui->alignCheckBox->setChecked(alignEnabled);
    ui->alignCheckBox->blockSignals(false);
}

void RomTemplateDialog::on_buildButton_clicked() {
    emit buildRequested();
}

void RomTemplateDialog::on_runTemplateDRCButton_clicked() {
    emit runTemplateDRCRequested();
}

void RomTemplateDialog::on_applyCorrectionsButton_clicked() {
    emit correctionsRequested(ui->confidenceSpinBox->value());
}

void RomTemplateDialog::on_nccThresholdSpinBox_valueChanged(double value) {
    RomRuleTemplate::LOW_NCC_THRESHOLD = value;
}

void RomTemplateDialog::on_searchRadiusSpinBox_valueChanged(int value) {
    RomBitTemplate::SEARCH_RADIUS = value;
    emit settingsChanged();
}

void RomTemplateDialog::on_templateWSpinBox_valueChanged(int value) {
    RomBitTemplate::TEMPLATE_W = value;
    emit settingsChanged();
}

void RomTemplateDialog::on_templateHSpinBox_valueChanged(int value) {
    RomBitTemplate::TEMPLATE_H = value;
    emit settingsChanged();
}

void RomTemplateDialog::on_alignCheckBox_toggled(bool checked) {
    emit alignEnabledChanged(checked);
    emit settingsChanged();
}
