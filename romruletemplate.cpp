#include "romruletemplate.h"
#include "rombittemplate.h"
#include "rombititem.h"
#include "maskromtool.h"

#include <QtConcurrent>

double RomRuleTemplate::LOW_NCC_THRESHOLD = 0.6;

void RomRuleTemplate::evaluate(MaskRomTool *mrt) {
    // Use the already-built template from mrt; skip silently if none has been built.
    if(!mrt->bitTemplate || !mrt->bitTemplate->isBuilt())
        return;

    const RomBitTemplate &tmpl = *mrt->bitTemplate;

    // Phase 1 — collect tasks on the main thread (getImage reads mrt->background).
    struct BitTask {
        RomBitItem *bit;       // kept for score writeback in Phase 3
        QPointF     pos;
        long        row, col;
        bool        currentValue;
        int         key;
        QImage      padded;
        bool        hasTmpl;
    };
    QVector<BitTask> tasks;

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
                int r = RomBitTemplate::SEARCH_RADIUS;
                tasks.append({ bit, bit->pos(), bit->row, bit->col,
                                bit->bitValue(), key,
                                bit->getImage(tmpl.templateW() + 2*r, tmpl.templateH() + 2*r),
                                tmpl.hasTemplate(key) });
            }
            prev = bit;
            bit  = bit->nexttoright;
        }
        rowfirst = rowfirst->nextrow;
    }

    // Phase 2 — parallel NCC evaluation (tmpl is read-only, no shared writes).
    struct ViolResult {
        RomBitItem *bit;
        bool        disagree, poor;
        QPointF     pos;
        long        row, col;
        bool        currentValue, vote;
        double      score;
    };

    double threshold = LOW_NCC_THRESHOLD;
    QList<ViolResult> results = QtConcurrent::blockingMapped(tasks,
        [&tmpl, threshold](const BitTask &t) -> ViolResult {
            auto [vote, score] = tmpl.voteBestWithScore(t.key, t.padded);
            return { t.bit,
                     vote != t.currentValue,
                     t.hasTmpl && score < threshold,
                     t.pos, t.row, t.col, t.currentValue, vote, score };
        });

    // Phase 3 — write scores back and add violations on the main thread.
    for(const auto &r : results) {
        r.bit->nccScore       = r.score;
        r.bit->nccDisagreement = r.disagree;
        r.bit->refreshBrush();

        if(r.disagree) {
            auto *v = new RomRuleViolation(r.pos,
                QString("Template disagrees at %1,%2").arg(r.row).arg(r.col),
                QString("Threshold=%1, template=%2, NCC=%3")
                    .arg(r.currentValue).arg(r.vote).arg(r.score,0,'f',3));
            v->error = true;
            mrt->addViolation(v);
        } else if(r.poor) {
            auto *v = new RomRuleViolation(r.pos,
                QString("Poor template match at %1,%2").arg(r.row).arg(r.col),
                QString("NCC=%1 (min %2)").arg(r.score,0,'f',3).arg(threshold));
            v->error = false;
            mrt->addViolation(v);
        }
    }
}
