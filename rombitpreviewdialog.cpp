#include "rombitpreviewdialog.h"
#include "rombititem.h"
#include "rombittemplate.h"
#include "romruletemplate.h"
#include "maskromtool_autogen/include/ui_rombitpreviewdialog.h"

#include <QPixmap>

RomBitPreviewDialog::RomBitPreviewDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RomBitPreviewDialog) {
    ui->setupUi(this);
}

RomBitPreviewDialog::~RomBitPreviewDialog() {
    delete ui;
}

void RomBitPreviewDialog::showBit(RomBitItem *bit, RomBitTemplate *tmpl) {
    if(!bit) return;

    QImage img = bit->getImage();
    if(img.isNull()) return;

    int sz = qMin(ui->previewLabel->width(), ui->previewLabel->height());
    if(sz < 1) sz = 160;
    QImage scaled = img.scaled(sz, sz, Qt::KeepAspectRatio, Qt::FastTransformation);
    ui->previewLabel->setPixmap(QPixmap::fromImage(scaled));

    // --- Info label ---
    QStringList lines;

    // Bit identity, value, forced flag
    QString location = (bit->row >= 0 && bit->col >= 0)
        ? QString("%1,%2").arg(bit->row).arg(bit->col)
        : QString("@%1,%2").arg(qRound(bit->x())).arg(qRound(bit->y()));
    QString bitLine = QString("Bit %1 = <b>%2</b>")
        .arg(location).arg(bit->bitValue() ? "1" : "0");
    if(bit->isFixed())
        bitLine += " (forced)";
    lines << bitLine;

    // NCC score + what the template voted
    if(bit->nccScore >= 0.0) {
        bool nccVote = bit->nccDisagreement ? !bit->bitValue() : bit->bitValue();
        QString nccStr = QString("NCC: %1 (").arg(bit->nccScore, 0, 'f', 3);
        if(bit->nccDisagreement)
            nccStr += QString("disagree, bit=%1)").arg(nccVote ? "1" : "0");
        else if(bit->nccScore < RomRuleTemplate::LOW_NCC_THRESHOLD)
            nccStr += "uncertain)";
        else
            nccStr += QString("confident, bit=%1)").arg(nccVote ? "1" : "0");
        lines << nccStr;
    } else if(tmpl && tmpl->isBuilt()) {
        lines << "NCC: not yet evaluated";
    }

    ui->infoLabel->setText(lines.join("<br>"));
}
