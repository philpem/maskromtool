#include "rombitpreviewdialog.h"
#include "rombititem.h"
#include "rombittemplate.h"
#include "romruletemplate.h"
#include "maskromtool.h"
#include "maskromtool_autogen/include/ui_rombitpreviewdialog.h"

#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>

RomBitPreviewDialog::RomBitPreviewDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RomBitPreviewDialog) {
    ui->setupUi(this);
}

RomBitPreviewDialog::~RomBitPreviewDialog() {
    delete ui;
}

void RomBitPreviewDialog::showBit(RomBitItem *bit, RomBitTemplate *tmpl, MaskRomTool *mrt) {
    if(!bit) return;

    m_srcImage = bit->getImage();
    if(m_srcImage.isNull()) return;

    m_mrt  = mrt;
    m_tmpl = tmpl;

    updatePreview();

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

void RomBitPreviewDialog::updatePreview() {
    if(m_srcImage.isNull()) return;

    int sz = qMin(ui->previewLabel->width(), ui->previewLabel->height());
    if(sz < 1) sz = 160;
    QImage scaled = m_srcImage.scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Read crop dimensions live so the box stays current as spinboxes change.
    int tw = 0, th = 0;
    if(m_tmpl && m_tmpl->isBuilt()) {
        tw = m_tmpl->templateW();
        th = m_tmpl->templateH();
    } else if(RomBitTemplate::TEMPLATE_W > 0 && RomBitTemplate::TEMPLATE_H > 0) {
        tw = RomBitTemplate::TEMPLATE_W;
        th = RomBitTemplate::TEMPLATE_H;
    } else if(m_mrt) {
        QRectF sr = m_mrt->sampler->getRect(m_mrt);
        tw = qMax(1, (int)qRound(qAbs(sr.width())));
        th = qMax(1, (int)qRound(qAbs(sr.height())));
    }

    if(tw > 0 && th > 0) {
        double scale = (double)scaled.width() / m_srcImage.width();
        int cx = scaled.width()  / 2;
        int cy = scaled.height() / 2;
        int rw = qRound(tw * scale);
        int rh = qRound(th * scale);
        QPainter p(&scaled);
        p.setPen(QPen(Qt::red, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(cx - rw/2, cy - rh/2, rw, rh);
    }

    ui->previewLabel->setPixmap(QPixmap::fromImage(scaled));
}

void RomBitPreviewDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    updatePreview();
}
