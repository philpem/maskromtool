#include "romruletemplate.h"
#include "rombittemplate.h"
#include "maskromtool.h"

double RomRuleTemplate::LOW_NCC_THRESHOLD = 0.6;

void RomRuleTemplate::evaluate(MaskRomTool *mrt) {
    RomBitTemplate tmpl;
    tmpl.build(mrt);

    if(!tmpl.isBuilt()) {
        auto *v = new RomRuleViolation(QPointF(0,0),
            "No fixed bits for templates",
            "Fix at least a few known-good bits with Shift+F before running this rule.");
        v->error = false;
        mrt->addViolation(v);
        return;
    }

    RomBitItem *rowfirst = mrt->markBitTable();
    while(rowfirst) {
        RomBitItem *prev = nullptr;
        RomBitItem *bit  = rowfirst;
        while(bit) {
            if(!bit->isFixed()) {
                bool leftVal  = prev ? prev->bitValue() : bit->bitValue();
                RomBitItem *next = bit->nexttoright;
                bool rightVal = next ? next->bitValue() : bit->bitValue();
                int key = RomBitTemplate::makeKey(leftVal, bit->bitValue(), rightVal);

                QImage raw    = bit->getImage();
                QImage padded = tmpl.paddedCrop(raw);

                bool templVote = tmpl.voteBest(key, padded);
                double score   = tmpl.hasTemplate(key)
                                 ? tmpl.nccBest(key, padded) : 0.0;

                if(templVote != bit->bitValue()) {
                    auto *v = new RomRuleViolation(bit->pos(),
                        QString("Template disagrees at %1,%2").arg(bit->row).arg(bit->col),
                        QString("Threshold=%1, template=%2, NCC=%3")
                            .arg(bit->bitValue()).arg(templVote).arg(score,0,'f',3));
                    v->error = true;
                    mrt->addViolation(v);
                } else if(tmpl.hasTemplate(key) && score < LOW_NCC_THRESHOLD) {
                    auto *v = new RomRuleViolation(bit->pos(),
                        QString("Poor template match at %1,%2").arg(bit->row).arg(bit->col),
                        QString("NCC=%1 (min %2)").arg(score,0,'f',3).arg(LOW_NCC_THRESHOLD));
                    v->error = false;
                    mrt->addViolation(v);
                }
            }
            prev = bit;
            bit  = bit->nexttoright;
        }
        rowfirst = rowfirst->nextrow;
    }
}
