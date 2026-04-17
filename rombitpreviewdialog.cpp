#include "rombitpreviewdialog.h"
#include "rombititem.h"
#include "rombittemplate.h"
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

    if(tmpl && tmpl->isBuilt()) {
        // We don't have neighbour context here, so use a simplified title.
        setWindowTitle(QString("Bit %1,%2 = %3")
            .arg(bit->row).arg(bit->col).arg(bit->bitValue() ? "1" : "0"));
    } else {
        setWindowTitle(QString("Bit %1,%2 = %3")
            .arg(bit->row).arg(bit->col).arg(bit->bitValue() ? "1" : "0"));
    }
}
