//
// Created by codetector on 8/29/19.
//

#ifndef TARGET_TL45_H
#define TARGET_TL45_H

#include <llvm/CodeGen/AsmPrinter.h>
#include "MCTargetDesc/TL45MCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
    class TargetMachine;
    class TL45TargetMachine;
    class MCInst;
    FunctionPass *createTL45ISelDag(TL45TargetMachine &TM, CodeGenOpt::Level OptLevel);
    bool LowerTL45MachineOperandToMCOperand(const MachineOperand &MO,
                                               MCOperand &MCOp,
                                               const AsmPrinter &AP);
} // end namespace llvm;

#endif

