#ifndef ROMBITPREVIEWDIALOG_H
#define ROMBITPREVIEWDIALOG_H

#include <QDialog>

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

    void showBit(RomBitItem *bit, RomBitTemplate *tmpl = nullptr);

private:
    Ui::RomBitPreviewDialog *ui;
};

#endif // ROMBITPREVIEWDIALOG_H
