//
// Created by codetector on 9/2/19.
//

#include "TL45.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/MC/MCStreamer.h"
#include "InstPrinter/TL45InstPrinter.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"
namespace {
class TL45AsmPrinter : public AsmPrinter {

public:
  TL45AsmPrinter(TargetMachine &TM,
                   std::unique_ptr<MCStreamer> &&Streamer) : AsmPrinter(TM, std::move(Streamer)) {}

  void EmitInstruction(const MachineInstr *) override;

  bool emitPseudoExpansionLowering(MCStreamer &OutStreamer,
                                   const MachineInstr *MI);

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode, raw_ostream &OS) override;

};
}

static MCOperand lowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym,
                                    const AsmPrinter &AP) {
  MCContext &Ctx = AP.OutContext;

  const MCExpr *ME =
          MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, Ctx);

  if (!MO.isJTI() && !MO.isMBB() && MO.getOffset())
    ME = MCBinaryExpr::createAdd(
            ME, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);

  return MCOperand::createExpr(ME);
}

static bool LowerTL45MachineOperandToMCOperand(const MachineOperand &MO,
                                               MCOperand &MCOp,
                                               const AsmPrinter &AP) {
  switch (MO.getType()) {
    default:
      report_fatal_error("LowerTL45MachineInstrToMCInst: unknown operand type");
    case MachineOperand::MO_Register:
      // Ignore all implicit register operands.
      if (MO.isImplicit())
        return false;
      MCOp = MCOperand::createReg(MO.getReg());
      break;
    case MachineOperand::MO_RegisterMask:
      // Regmasks are like implicit defs.
      return false;
    case MachineOperand::MO_Immediate:
      MCOp = MCOperand::createImm(MO.getImm());
      break;
    case MachineOperand::MO_MachineBasicBlock:
      MCOp = lowerSymbolOperand(MO, MO.getMBB()->getSymbol(), AP);
      break;
    case MachineOperand::MO_GlobalAddress:
      MCOp = lowerSymbolOperand(MO, AP.getSymbol(MO.getGlobal()), AP);
      break;
    case MachineOperand::MO_BlockAddress:
      MCOp = lowerSymbolOperand(
              MO, AP.GetBlockAddressSymbol(MO.getBlockAddress()), AP);
      break;
    case MachineOperand::MO_ExternalSymbol:
      MCOp = lowerSymbolOperand(
              MO, AP.GetExternalSymbolSymbol(MO.getSymbolName()), AP);
      break;
    case MachineOperand::MO_ConstantPoolIndex:
      MCOp = lowerSymbolOperand(MO, AP.GetCPISymbol(MO.getIndex()), AP);
      break;
  }
  return true;
}

// adapted from RISCV target.
bool TL45AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &OS) {
  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
    return false;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0)
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    case 'z':      // Print zero register if zero, regular printing otherwise.
      if (MO.isImm() && MO.getImm() == 0) {
        OS << TL45InstPrinter::getRegisterName(TL45::r0);
        return false;
      }
      break;
    case 'i': // Literal 'i' if operand is not a register.
      if (!MO.isReg())
        OS << 'i';
      return false;
    }
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  case MachineOperand::MO_Register:
    OS << TL45InstPrinter::getRegisterName(MO.getReg());
    return false;
  default:
    break;
  }

  return true;
}

// Simple pseudo-instructions have their lowering (with expansion to real
// instructions) auto-generated.
#include "TL45GenMCPseudoLowering.inc"

void TL45AsmPrinter::EmitInstruction(const MachineInstr *MI) {
  if (emitPseudoExpansionLowering(*OutStreamer, MI))
    return;

  MCInst TmpInst;

  TmpInst.setOpcode(MI->getOpcode());

  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp;
    if (LowerTL45MachineOperandToMCOperand(MO, MCOp, *this))
      TmpInst.addOperand(MCOp);
  }

  EmitToStreamer(*OutStreamer, TmpInst);
}

extern "C" void LLVMInitializeTL45AsmPrinter() {
  RegisterAsmPrinter<TL45AsmPrinter> X(TheTL45Target);
}