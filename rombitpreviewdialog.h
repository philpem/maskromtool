#ifndef ROMBITPREVIEWDIALOG_H
#define ROMBITPREVIEWDIALOG_H

#include <QDialog>
#include <QImage>

class MaskRomTool;
class RomBitItem;
class RomBitTemplate;

namespace Ui {
class RomBitPreviewDialog;
}

class RomBitPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit RomBitPreviewDialog(QWidget *parent = nullptr);
    ~RomBitPreviewDialog();

    void showBit(RomBitItem *bit, RomBitTemplate *tmpl = nullptr, MaskRomTool *mrt = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updatePreview();

    Ui::RomBitPreviewDialog *ui;
    QImage       m_srcImage;
    MaskRomTool *m_mrt = nullptr;
    RomBitTemplate *m_tmpl = nullptr;
};

#endif // ROMBITPREVIEWDIALOG_H
