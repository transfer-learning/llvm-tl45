
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include <llvm/CodeGen/SelectionDAGNodes.h>
#include <llvm/CodeGen/RegisterScavenging.h>

#include "TL45InstrInfo.h"
#include "MCTargetDesc/TL45MCTargetDesc.h"

#define DEBUG_TYPE "tl45-instr-info"

#define GET_INSTRINFO_CTOR_DTOR

#include "TL45GenInstrInfo.inc"

using namespace llvm;

TL45InstrInfo::TL45InstrInfo() : TL45GenInstrInfo(TL45::ADJCALLSTACKDOWN, TL45::ADJCALLSTACKUP, -1, TL45::RET), RI() {}

void TL45InstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        unsigned SrcReg, bool IsKill, int FI,
                                        const TargetRegisterClass *RC,
                                        const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  unsigned Opcode;

  if (TL45::GRRegsRegClass.hasSubClassEq(RC))
    Opcode = TL45::SW;
  else
    llvm_unreachable("Can't store this register to stack slot");

  BuildMI(MBB, I, DL, get(Opcode))
      .addReg(SrcReg)
      .addFrameIndex(FI)
      .addImm(0);
}

void TL45InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, unsigned DstReg,
    int FI, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  unsigned Opcode;

  if (TL45::GRRegsRegClass.hasSubClassEq(RC))
    Opcode = TL45::LW;
  else
    llvm_unreachable("Can't load this register from stack slot");

  BuildMI(MBB, I, DL, get(Opcode), DstReg).addFrameIndex(FI).addImm(0);
}

unsigned TL45InstrInfo::resolveComparison(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          const DebugLoc &DL,
                                          ISD::CondCode ConditionCode,
                                          MachineOperand &a,
                                          MachineOperand &b,
                                          unsigned int &JmpOpcode, bool isImm) const {
  unsigned bytesAdded;
  // Recursive rewrite rules :^)))
  // a =  b  ==> !(a != b) ==> skpe a, b; jmp 1; jmp dst
  // a >  b  ==> b < a     ==> skplt b, a; jmp 1; jmp dst
  // a >= b  ==>               skplt a, b; jmp dst
  // a <  b  ==> !(a >= b) ==> skplt a, b; jmp 1; jmp dst
  // a <= b  ==> b >= a    ==>
  // a != b  ==> skpe a, b; jmp dst

  if (isImm) {
    BuildMI(MBB, I, DL, get(TL45::SUB_TERMI)).addReg(TL45::r0).addReg(a.getReg()).addImm(b.getImm());
  } else {
    BuildMI(MBB, I, DL, get(TL45::SUB_TERM)).addReg(TL45::r0).addReg(a.getReg()).addReg(b.getReg());
  }

  bytesAdded += 4;

  switch (ConditionCode) {
  case ISD::CondCode::SETEQ:
  case ISD::CondCode::SETUEQ:
    JmpOpcode = TL45::JE;
    break;
  case ISD::CondCode::SETGT:
    JmpOpcode = TL45::JG;
    break;
  case ISD::CondCode::SETUGT:
    JmpOpcode = TL45::JA;
    break;
  case ISD::CondCode::SETUGE:
    JmpOpcode = TL45::JNB;
    break;
  case ISD::CondCode::SETGE:
    JmpOpcode = TL45::JGE;
    break;
  case ISD::CondCode::SETLT:
    JmpOpcode = TL45::JL;
    break;
  case ISD::CondCode::SETULT:
    JmpOpcode = TL45::JB;
    break;
  case ISD::CondCode::SETLE:
    JmpOpcode = TL45::JLE;
    break;
  case ISD::CondCode::SETULE:
    JmpOpcode = TL45::JBE;
    break;
  case ISD::CondCode::SETNE:
  case ISD::CondCode::SETUNE:
    JmpOpcode = TL45::JNE;
    break;
  default:
    LLVM_DEBUG(dbgs() << "Got cond code: " << ConditionCode << "\n");
    llvm_unreachable("dude weed lmao: how did we get such a condition code in "
                     "this pseudo instruction?! are we fLoATiNg?");
  }
  return bytesAdded;
}

bool TL45InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  const DebugLoc DL = MI.getDebugLoc();
  MachineBasicBlock &MBB = *MI.getParent();
  switch (MI.getOpcode()) {
  default:
    return false;
  case TL45::BrOff: {
    BuildMI(MBB, MI, DL, get(TL45::JMP))
        .add(MI.getOperand(0)) // Base
        .add(MI.getOperand(1)); // Offset
    break;
  }
  case TL45::CMP_JMP: {
    auto ConditionCode = ISD::CondCode(MI.getOperand(0).getImm());
    unsigned int JmpOpcode;
    resolveComparison(MBB, MI, DL, ConditionCode, MI.getOperand(1), MI.getOperand(2), JmpOpcode, false);
    BuildMI(MBB, MI, DL, get(JmpOpcode)).add(MI.getOperand(3)).add(MI.getOperand(4));
    break;
  }

  case TL45::CMPI_JMP: {
    auto ConditionCode = ISD::CondCode(MI.getOperand(0).getImm());
    unsigned int JmpOpcode;
    resolveComparison(MBB, MI, DL, ConditionCode, MI.getOperand(1), MI.getOperand(2), JmpOpcode, true);
    BuildMI(MBB, MI, DL, get(JmpOpcode)).add(MI.getOperand(3)).add(MI.getOperand(4));
    break;
  }

  case TL45::ADD32: {
    MachineOperand &dst = MI.getOperand(0);
    MachineOperand &src = MI.getOperand(1);
    MachineOperand &imm = MI.getOperand(2);

    if (imm.isImm()) {
      uint64_t val = (uint32_t) imm.getImm();

      uint64_t low = val & 0xffffu;
      uint64_t high = (val >> 16u) & 0xffffu;

      BuildMI(MBB, MI, DL, get(TL45::ADDIZ)).add(dst).add(src).addImm(low);
      BuildMI(MBB, MI, DL, get(TL45::ADDHI)).add(dst).add(dst).addImm(high);

    } else if (imm.isGlobal()) {
      auto gVal = imm.getGlobal();
      BuildMI(MBB, MI, DL, get(TL45::ADDIZ)).add(dst).add(src).addGlobalAddress(gVal);
      BuildMI(MBB, MI, DL, get(TL45::ADDHI)).add(dst).add(dst).addGlobalAddress(gVal);
    } else {
      report_fatal_error("ADD32 requires imm or global value");
    }

    break;
  } // TL45::ADD32
  }

  MBB.erase(MI);
  return true;
}

unsigned TL45InstrInfo::loadImmediate(unsigned FrameReg, int64_t Imm,
                                      MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator II,
                                      const DebugLoc &DL,
                                      unsigned &NewImm) const {

  llvm_unreachable("not yet implemented loadImmediate()");
}

bool TL45InstrInfo::validImmediate(unsigned Opcode, unsigned Reg,
                                   int64_t Amount) {
  switch (Opcode) {
  case TL45::ADDHI:
  case TL45::ADDIZ:
    return isUInt<16>(Amount);
  default: {
    return isInt<16>(Amount);
  }
  }
}

void TL45InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI,
                                const DebugLoc &DL,
                                unsigned DestReg, unsigned SrcReg,
                                bool KillSrc) const {
  BuildMI(MBB, MI, DL, get(TL45::ADD))
      .addReg(DestReg)
      .addReg(SrcReg)
      .addReg(TL45::r0);
}

/// Copied from header
/// Insert branch code into the end of the specified MachineBasicBlock. The
/// operands to this method are the same as those returned by AnalyzeBranch.
/// This is only invoked in cases where AnalyzeBranch returns success. It
/// returns the number of instructions inserted. If \p BytesAdded is non-null,
/// report the change in code size from the added instructions.
///
/// It is also invoked by tail merging to add unconditional branches in
/// cases where AnalyzeBranch doesn't apply because there was no original
/// branch to analyze.  At least this much must be implemented, else tail
/// merging needs to be disabled.
///
/// The CFG information in MBB.Predecessors and MBB.Successors must be valid
/// before calling this function.
unsigned TL45InstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  unsigned initial_vreg_count = MRI.getNumVirtRegs();

  if (Cond.empty()) {
    // Unconditional Branch, so we build a LdAH and JMP
    assert(!FBB && "Unconditional branch with multiple successors!");
    RegScavenger scavenger;
    unsigned ScratchReg = MRI.createVirtualRegister(&TL45::GRRegsRegClass);
    auto FirstMI = BuildMI(&MBB, DL, get(TL45::ADDHI), ScratchReg).addReg(TL45::r0).addMBB(TBB);
    BuildMI(&MBB, DL, get(TL45::JMP)).addReg(ScratchReg, RegState::Kill).addMBB(TBB);
    scavenger.enterBasicBlockEnd(MBB);
    auto SubReg = scavenger.scavengeRegisterBackwards(TL45::GRRegsRegClass, FirstMI->getIterator(), false, 0);
    MRI.replaceRegWith(ScratchReg, SubReg);
    if (initial_vreg_count == 0) {
      MRI.clearVirtRegs();
    }

    if (BytesAdded)
      *BytesAdded += 8;
    return 1;
  }

  unsigned Count = 0;

  report_fatal_error("Can't insert conditional branch yet");
  unsigned JmpOpc = Cond[0].getImm();
  if (JmpOpc == TL45::CMP_JMP || JmpOpc == TL45::CMPI_JMP) {

    BuildMI(MBB, MBB.end(), DL, get(JmpOpc)).add(Cond.slice(1)).addMBB(TBB);
    Count++;

  } else {

    // condition
    BuildMI(MBB, MBB.end(), DL, get(Cond[1].getImm()), TL45::r0).add(Cond[2]).add(Cond[3]);
    Count++;

    // True branch instruction
    BuildMI(MBB, MBB.end(), DL, get(Cond[0].getImm())).addMBB(TBB);
    Count++;
  }

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(MBB, MBB.end(), DL, get(TL45::JMPI)).addMBB(FBB);
    Count++;
  }

  if (BytesAdded)
    *BytesAdded += (int) Count * 4;
  return Count;
}

static unsigned int BrOpcode[] = {
    TL45::JO, TL45::JOI, TL45::JNO, TL45::JNOI, TL45::JS, TL45::JSI, TL45::JNS, TL45::JNSI,
    TL45::JE, TL45::JEI, TL45::JNE, TL45::JNEI, TL45::JB, TL45::JBI, TL45::JNB, TL45::JNBI,

    TL45::JBE, TL45::JBEI, TL45::JA, TL45::JAI, TL45::JL, TL45::JLI, TL45::JGE, TL45::JGEI,
    TL45::JLE, TL45::JLEI, TL45::JG, TL45::JGI,
    TL45::JMP,
    TL45::JMPI,

//    TL45::BrOff,

    0
};

static bool getAnalyzableBrOpc(unsigned Opc) {

  for (unsigned o : BrOpcode) {
    if (o == Opc) {
      return true;
    }
  }

  return Opc == TL45::CMP_JMP || Opc == TL45::CMPI_JMP;

}

static void AnalyzeCondBr(const MachineInstr *Inst, unsigned Opc,
                          MachineBasicBlock *&BB,
                          SmallVectorImpl<MachineOperand> &Cond,
                          MachineBasicBlock::reverse_iterator LastInstI) {
  assert(getAnalyzableBrOpc(Opc) && "Not an analyzable branch");
  unsigned NumOp = Inst->getNumExplicitOperands();

  // for both int and fp branches, the last explicit operand is the
  // MBB.
  BB = Inst->getOperand(NumOp - 1).getMBB();
  // Cond.push_back(MachineOperand::CreateImm(Opc));

  if (Opc == TL45::CMP_JMP || Opc == TL45::CMPI_JMP) {

    Cond.push_back(MachineOperand::CreateImm(Inst->getOpcode()));

    for (unsigned i = 0; i < NumOp - 1; i++)
      Cond.push_back(Inst->getOperand(i));

  } else {

//  for (int i = 0; i < NumOp-1; i++)
//    Cond.push_back(Inst->getOperand(i));

    MachineInstr &LastInst = *LastInstI;

    assert((LastInst.getOpcode() == TL45::SUB_TERM || LastInst.getOpcode() == TL45::SUB_TERMI) && "unknown predicate");

    // This is based heavily off of RISCV parseCondBranch()

    // jump opcode
    Cond.push_back(MachineOperand::CreateImm(Inst->getOpcode()));

    // SUB/SUBI
    Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode()));

    // SUBI r0, r5, 4
    //           ^  ^
    Cond.push_back(LastInst.getOperand(1));
    Cond.push_back(LastInst.getOperand(2));
  }
}

/// Copied From TargetInstrInfo.h:
///
/// Analyze the branching code at the end of MBB, returning
/// true if it cannot be understood (e.g. it's a switch dispatch or isn't
/// implemented for a target).  Upon success, this returns false and returns
/// with the following information in various cases:
///
/// 1. If this block ends with no branches (it just falls through to its succ)
///    just return false, leaving TBB/FBB null.
/// 2. If this block ends with only an unconditional branch, it sets TBB to be
///    the destination block.
/// 3. If this block ends with a conditional branch and it falls through to a
///    successor block, it sets TBB to be the branch destination block and a
///    list of operands that evaluate the condition. These operands can be
///    passed to other TargetInstrInfo methods to create new branches.
/// 4. If this block ends with a conditional branch followed by an
///    unconditional branch, it returns the 'true' destination in TBB, the
///    'false' destination in FBB, and a list of operands that evaluate the
///    condition.  These operands can be passed to other TargetInstrInfo
///    methods to create new branches.
///
/// Note that removeBranch and insertBranch must be implemented to support
/// cases where this method returns success.
///
/// If AllowModify is true, then this routine is allowed to modify the basic
/// block (e.g. delete instructions after the unconditional branch).
///
/// The CFG information in MBB.Predecessors and MBB.Successors must be valid
/// before calling this function.
bool TL45InstrInfo::analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB, MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond, bool AllowModify) const {
  return true;
  MachineBasicBlock::reverse_iterator I = MBB.rbegin(), REnd = MBB.rend();

  // Skip all the debug instructions.
  while (I != REnd && I->isDebugInstr())
    ++I;

  if (I == REnd || !isUnpredicatedTerminator(*I)) {
    // This block ends with no branches (it just falls through to its succ).
    // Leave TBB/FBB null.
    TBB = FBB = nullptr;
    return false;
  }

  SmallVector<MachineInstr *, 4> BranchInstrs;

  MachineInstr *LastInst = &*I;
  unsigned LastOpc = LastInst->getOpcode();
  BranchInstrs.push_back(LastInst);


  // Not an analyzable branch (e.g., indirect jump).
  if (!getAnalyzableBrOpc(LastOpc))
    return true;

  // Get the second to last instruction in the block.
  unsigned SecondLastOpc = 0;
  MachineInstr *SecondLastInst = nullptr;

  // Skip past any debug instruction to see if the second last actual
  // is a branch.
  ++I;
  while (I != REnd && I->isDebugInstr())
    ++I;

  if (I != REnd) {
    SecondLastInst = &*I;
    SecondLastOpc = SecondLastInst->getOpcode();

    // Not an analyzable branch (must be an indirect jump).
    if (isUnpredicatedTerminator(*SecondLastInst) && !getAnalyzableBrOpc(SecondLastOpc))
      return true;
  }

  // If there is only one terminator instruction, process it.
  if (!SecondLastOpc || !getAnalyzableBrOpc(SecondLastOpc)) {
    // Unconditional branch.
    if (LastInst->isUnconditionalBranch()) {
      switch (LastInst->getOpcode()) {
      default:
        llvm_unreachable("Unexpected Unconditional Branch");
        break;
      case TL45::JMPI: {
        report_fatal_error("Found a JMPI while analyzing branches", false);
        TBB = LastInst->getOperand(0).getMBB();
        break;
      }
      case TL45::BrOff: {
        TBB = LastInst->getOperand(1).getMBB();
        break;
      }
      case TL45::JMP: {
        TBB = LastInst->getOperand(1).getMBB();
        break;
      }
      }
      return false;
    }

    // Conditional branch
    AnalyzeCondBr(LastInst, LastOpc, TBB, Cond, I);
    return false;
  }

  // If we reached here, there are two branches.
  // If there are three terminators, we don't know what sort of block this is.
  if (++I != REnd && isUnpredicatedTerminator(*I))
    return true;

  BranchInstrs.insert(BranchInstrs.begin(), SecondLastInst);

  // If second to last instruction is an unconditional branch,
  // analyze it and remove the last instruction.
  if (SecondLastInst->isUnconditionalBranch()) {
    if (!AllowModify) {
      return true;
    }

    TBB = SecondLastInst->getOperand(0).getMBB();
    LastInst->eraseFromParent();
    BranchInstrs.pop_back();
    return false;
  }

  // Conditional branch followed by an unconditional branch.
  // The last one must be unconditional.
  if (!LastInst->isUnconditionalBranch())
    return true;

  AnalyzeCondBr(SecondLastInst, SecondLastOpc, TBB, Cond, I);
  FBB = LastInst->getOperand(0).getMBB();

  return false;
}

unsigned TL45InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");

  MachineBasicBlock::reverse_iterator I = MBB.rbegin(), REnd = MBB.rend();
  unsigned removed = 0;

  // Up to 2 branches are removed.
  // Note that indirect branches are not removed.
  while (I != REnd && removed < 2) {
    // Skip past debug instructions.
    if (I->isDebugInstr()) {
      ++I;
      continue;
    }
    if (!getAnalyzableBrOpc(I->getOpcode()))
      break;
    // Remove the branch.
    if (I->getOpcode() == TL45::BrOff || I->getOpcode() == TL45::JMP) {
      I->eraseFromParent();
      I = MBB.rbegin();
      ++removed;
      if (I->getOpcode() == TL45::ADDHI || I->getOpcode() == TL45::LdAH) {
        I->eraseFromParent();
        I = MBB.rbegin();
        ++removed;
      }
    } else {
      I->eraseFromParent();
      I = MBB.rbegin();
      ++removed;
    }
  }
  return removed;
}
