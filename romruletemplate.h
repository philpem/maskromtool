#ifndef ROMRULETEMPLATE_H
#define ROMRULETEMPLATE_H

#include "romrule.h"

class RomRuleTemplate : public RomRule {
public:
    static double LOW_NCC_THRESHOLD;  // default 0.6, configurable via RomTemplateDialog
    void evaluate(MaskRomTool *mrt) override;
};

#endif // ROMRULETEMPLATE_H
