#include <iostream>
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetOptions.h"

#include "TL45FrameLowering.h"
#include "TL45Subtarget.h"

#define DEBUG_TYPE "tl45-frame-lowering"

using namespace llvm;
//
//static unsigned materializeOffset(MachineFunction &MF, MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, unsigned Offset) {
//	const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
//	DebugLoc dl = MBBI != MBB.end() ? MBBI->getDebugLoc() :
//	DebugLoc();
//	const uint64_t MaxSubImm = 0xfff;
//	if (Offset <= MaxSubImm) {
//		return 0;
//	}
//	else {
//		unsigned OffsetReg = TOY::R2;
//		unsigned OffsetLo = (unsigned)(Offset & 0xffff);
//		unsigned OffsetHi = (unsigned)((Offset & 0xffff0000) >> 16);
//		BuildMI(MBB, MBBI, dl, TII.get(TOY::MOVLOi16), OffsetReg)
//		.addImm(OffsetLo)
//		.setMIFlag(MachineInstr::FrameSetup);
//		if (OffsetHi) {
//			BuildMI(MBB, MBBI, dl, TII.get(TOY::MOVHIi16), OffsetReg)
//			.addReg(OffsetReg)
//			.addImm(OffsetHi)
//			.setMIFlag(MachineInstr::FrameSetup);
//		}
//	return OffsetReg;
//	}
//}
//
//
//uint64_t TL45FrameLowering::computeStackSize(MachineFunction &MF) const {
//	MachineFrameInfo *MFI = MF.getFrameInfo();
//	uint64_t StackSize = MFI->getStackSize();
//	unsigned StackAlign = getStackAlignment();
//	if (StackAlign > 0) {
//		StackSize = RoundUpToAlignment(StackSize, StackAlign);
//	}
//	return StackSize;
//}

void TL45FrameLowering::determineFrameLayout(MachineFunction &MF, bool hasFP) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TL45RegisterInfo *RI = STI.getRegisterInfo();

  // Get the number of bytes to allocate from the FrameInfo.
  uint64_t FrameSize = MFI.getStackSize();

  // Get the alignment.
  unsigned StackAlign = getStackAlignment();
  if (RI->needsStackRealignment(MF)) {
    unsigned MaxStackAlign = std::max(StackAlign, MFI.getMaxAlignment());
    FrameSize += (MaxStackAlign - StackAlign);
    StackAlign = MaxStackAlign;
  }

  // Set Max Call Frame Size
  uint64_t MaxCallSize = alignTo(MFI.getMaxCallFrameSize(), StackAlign);
  MFI.setMaxCallFrameSize(MaxCallSize);

  // Make sure the frame is aligned.
  FrameSize = alignTo(FrameSize, StackAlign);

  if (hasFP) {
    auto TMDL = MF.getDataLayout();
    uint64_t ptr_size = TMDL.getPointerSizeInBits() / TMDL.getBitsPerMemoryUnit();
    FrameSize += ptr_size;
    LLVM_DEBUG(dbgs() << "[TL45FrameLowering::determineFrameLayout] MF: '"
    << MF.getName() << "' has frame pointer, incrementing stack frame size by ptr_size.\n");
  }

  // Update frame info.
  MFI.setStackSize(FrameSize);
}

void TL45FrameLowering::adjustReg(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   const DebugLoc &DL, Register DestReg,
                                   Register SrcReg, int64_t Val,
                                   MachineInstr::MIFlag Flag) const {
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  const TL45InstrInfo *TII = STI.getInstrInfo();

  if (DestReg == SrcReg && Val == 0)
    return;

  if (isInt<16>(Val)) {
    BuildMI(MBB, MBBI, DL, TII->get(TL45::ADDI), DestReg)
            .addReg(SrcReg)
            .addImm(Val)
            .setMIFlag(Flag);
  } else if (isInt<32>(Val)) {
    llvm_unreachable("lol can't do yet");
  } else {
    report_fatal_error("adjustReg cannot yet handle adjustments >32 bits");
  }
}

void TL45FrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  assert(&MF.front() == &MBB && "Shrink-wrapping not yet supported");

  bool hasFramePointer = hasFP(MF);

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TL45RegisterInfo *RI = STI.getRegisterInfo();
  const TL45InstrInfo *TII = STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();


  LLVM_DEBUG(dbgs() << "Emitting Prologue for function: " << MF.getName() << "\n");

  if (RI->needsStackRealignment(MF) && MFI.hasVarSizedObjects()) {
    report_fatal_error(
            "TL-45 backend can't currently handle functions that need stack "
            "realignment and have variable sized objects");
  }

  if (MFI.hasVarSizedObjects()) {
    LLVM_DEBUG(dbgs() << "Detected Variable Sized Items on stack");
  }

  Register FPReg = TL45::bp;
  Register SPReg = TL45::sp;

  // Debug location must be unknown since the first debug location is used
  // to determine the end of the prologue.
  DebugLoc DL;

  // Determine the correct frame layout
  determineFrameLayout(MF, hasFramePointer);

  // FIXME (note copied from Lanai): This appears to be overallocating.  Needs
  // investigation. Get the number of bytes to allocate from the FrameInfo.
  uint64_t StackSize = MFI.getStackSize();

  // Early exit if there is no need to allocate on the stack
  if (StackSize == 0 && !MFI.adjustsStack())
    return;

  // Allocate space on the stack if necessary.
  // aka Decrement stack pointer
  adjustReg(MBB, MBBI, DL, SPReg, SPReg, -StackSize, MachineInstr::FrameSetup);

  // Emit ".cfi_def_cfa_offset StackSize"
  unsigned CFIIndex = MF.addFrameInst(
          MCCFIInstruction::createDefCfaOffset(nullptr, -StackSize));
  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);

  // The frame pointer is callee-saved, and code has been generated for us to
  // save it to the stack. We need to skip over the storing of callee-saved
  // registers as the frame pointer must be modified after it has been saved
  // to the stack, not before.
  // FIXME: assumes exactly one instruction is used to save each callee-saved
  // register.
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  std::advance(MBBI, CSI.size());

  // Iterate over list of callee-saved registers and emit .cfi_offset
  // directives.
  for (const auto &Entry : CSI) {
    int64_t Offset = MFI.getObjectOffset(Entry.getFrameIdx());
    Register Reg = Entry.getReg();
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
            nullptr, RI->getDwarfRegNum(Reg, true), Offset));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
            .addCFIIndex(CFIIndex);
  }

  // Generate new FP.
  if (hasFramePointer) {
    BuildMI(MBB, MBBI, DL, TII->get(TL45::SW)).addReg(TL45::bp).addReg(TL45::sp).addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(TL45::ADDI)).addReg(TL45::bp).addReg(TL45::sp)
    .addImm(0).setMIFlag(MachineInstr::FrameSetup);

//    adjustReg(MBB, MBBI, DL, FPReg, SPReg,
//              StackSize - RVFI->getVarArgsSaveSize(), MachineInstr::FrameSetup);
//    adjustReg(MBB, MBBI, DL, FPReg, SPReg, StackSize, MachineInstr::FrameSetup);

    // Emit ".cfi_def_cfa $fp, 0"
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createDefCfa(
            nullptr, RI->getDwarfRegNum(FPReg, true), 0));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
            .addCFIIndex(CFIIndex);

    // Realign Stack
    const TL45RegisterInfo *RI = STI.getRegisterInfo();
    if (RI->needsStackRealignment(MF)) {
      // llvm_unreachable("cannot realign");
      unsigned MaxAlignment = MFI.getMaxAlignment();

      const TL45InstrInfo *TII = STI.getInstrInfo();
      if (isInt<12>(-(int)MaxAlignment)) {
        BuildMI(MBB, MBBI, DL, TII->get(TL45::ANDI), SPReg)
                .addReg(SPReg)
                .addImm(-(int)MaxAlignment);
      } else {
         llvm_unreachable("cannot realign");
      }
    }
  }
}

void TL45FrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  bool hasFramePointer = hasFP(MF);

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  const TL45RegisterInfo *RI = STI.getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
//  auto *RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  DebugLoc DL = MBBI->getDebugLoc();
  const TL45InstrInfo *TII = STI.getInstrInfo();
  Register FPReg = TL45::bp;
  Register SPReg = TL45::sp;

  // Skip to before the restores of callee-saved registers
  // FIXME: assumes exactly one instruction is used to restore each
  // callee-saved register.
  auto LastFrameDestroy = std::prev(MBBI, MFI.getCalleeSavedInfo().size());

  uint64_t StackSize = MFI.getStackSize();

//  uint64_t FPOffset = StackSize;// - RVFI->getVarArgsSaveSize();

  // Restore the stack pointer using the value of the frame pointer. Only
  // necessary if the stack pointer was modified, meaning the stack size is
  // unknown.
  if (RI->needsStackRealignment(MF) || MFI.hasVarSizedObjects()) {
    assert(hasFramePointer && "frame pointer should not have been eliminated");
//    adjustReg(MBB, LastFrameDestroy, DL, SPReg, FPReg, -FPOffset,
//              MachineInstr::FrameDestroy);
    // TODO Fixme
    BuildMI(MBB, LastFrameDestroy, DL, TII->get(TL45::ADDI)).addReg(SPReg).addReg(FPReg).addImm(-(StackSize - 4));
  }

  if (hasFramePointer) {

    // BP is a linked list!!!
    BuildMI(MBB, LastFrameDestroy, DL, TII->get(TL45::LW)).addReg(FPReg).addReg(FPReg).addImm(0);

  }

  // Add CFI directives for callee-saved registers.
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  // Iterate over list of callee-saved registers and emit .cfi_restore
  // directives.
  for (const auto &Entry : CSI) {
    Register Reg = Entry.getReg();
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createRestore(
            nullptr, RI->getDwarfRegNum(Reg, true)));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
            .addCFIIndex(CFIIndex);
  }

  // Deallocate stack
  adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackSize, MachineInstr::FrameDestroy);


  // After restoring $sp, we need to adjust CFA to $(sp + 0)
  // Emit ".cfi_def_cfa_offset 0"
  unsigned CFIIndex =
          MF.addFrameInst(MCCFIInstruction::createDefCfaOffset(nullptr, 0));
  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);
}

bool TL45FrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();

  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
         TRI->needsStackRealignment(MF);
}

int TL45FrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI, unsigned &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();

  int Offset = MFI.getObjectOffset(FI) - getOffsetOfLocalArea() +
               MFI.getOffsetAdjustment();


  FrameReg = RI->getFrameRegister(MF);
  if (hasFP(MF))
    Offset += 0;
  else
    Offset += MF.getFrameInfo().getStackSize();

  return Offset;
//  int offset = TargetFrameLowering::getFrameIndexReference(MF, FI, FrameReg);
//  assert(offset % 4 == 0 && "4-byte addressibility");
//  return offset / 4;
}


// Eliminate ADJCALLSTACKDOWN, ADJCALLSTACKUP pseudo instructions.
MachineBasicBlock::iterator TL45FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  Register SPReg = TL45::sp;
  DebugLoc DL = MI->getDebugLoc();

  if (!hasReservedCallFrame(MF)) {
    // If space has not been reserved for a call frame, ADJCALLSTACKDOWN and
    // ADJCALLSTACKUP must be converted to instructions manipulating the stack
    // pointer. This is necessary when there is a variable length stack
    // allocation (e.g. alloca), which means it's not possible to allocate
    // space for outgoing arguments from within the function prologue.
    int64_t Amount = MI->getOperand(0).getImm();

    if (Amount != 0) {
      // Ensure the stack remains aligned after adjustment.
      Amount = alignSPAdjust(Amount);

      if (MI->getOpcode() == TL45::ADJCALLSTACKDOWN)
        Amount = -Amount;

      adjustReg(MBB, MI, DL, SPReg, SPReg, Amount, MachineInstr::NoFlags);
    }
  }

  return MBB.erase(MI);
}