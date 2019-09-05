
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include "LC2200InstrInfo.h"
#include "MCTargetDesc/LC2200MCTargetDesc.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "LC2200GenInstrInfo.inc"

using namespace llvm;

LC2200InstrInfo::LC2200InstrInfo() : LC2200GenInstrInfo(), RI() {

}

unsigned
LC2200InstrInfo::loadImmediate(unsigned FrameReg, int64_t Imm, MachineBasicBlock &MBB, MachineBasicBlock::iterator II,
                               const DebugLoc &DL, unsigned &NewImm) const {

  llvm_unreachable("not yet implemented loadImmediate()");
//  //
//  // given original instruction is:
//  // Instr rx, T[offset] where offset is too big.
//  //
//  // lo = offset & 0xFFFF
//  // hi = ((offset >> 16) + (lo >> 15)) & 0xFFFF;
//  //
//  // let T = temporary register
//  // li T, hi
//  // shl T, 16
//  // add T, Rx, T
//  //
//  RegScavenger rs;
//  int32_t lo = Imm & 0xFFFF;
//  NewImm = lo;
//  int Reg =0;
//  int SpReg = 0;
//
//  rs.enterBasicBlock(MBB);
//  rs.forward(II);
//  //
//  // We need to know which registers can be used, in the case where there
//  // are not enough free registers. We exclude all registers that
//  // are used in the instruction that we are helping.
//  //  // Consider all allocatable registers in the register class initially
//  BitVector Candidates =
//          RI.getAllocatableSet
//                  (*II->getParent()->getParent(), &LC2200::GRRegsRegClass);
//  // Exclude all the registers being used by the instruction.
//  for (unsigned i = 0, e = II->getNumOperands(); i != e; ++i) {
//    MachineOperand &MO = II->getOperand(i);
//    if (MO.isReg() && MO.getReg() != 0 && !MO.isDef() &&
//        !Register::isVirtualRegister(MO.getReg()))
//      Candidates.reset(MO.getReg());
//  }
//
//  // If the same register was used and defined in an instruction, then
//  // it will not be in the list of candidates.
//  //
//  // we need to analyze the instruction that we are helping.
//  // we need to know if it defines register x but register x is not
//  // present as an operand of the instruction. this tells
//  // whether the register is live before the instruction. if it's not
//  // then we don't need to save it in case there are no free registers.
//  int DefReg = 0;
//  for (unsigned i = 0, e = II->getNumOperands(); i != e; ++i) {
//    MachineOperand &MO = II->getOperand(i);
//    if (MO.isReg() && MO.isDef()) {
//      DefReg = MO.getReg();
//      break;
//    }
//  }
//
//  BitVector Available = rs.getRegsAvailable(&LC2200::CPU16RegsRegClass);
//  Available &= Candidates;
//  //
//  // we use T0 for the first register, if we need to save something away.
//  // we use T1 for the second register, if we need to save something away.
//  //
//  unsigned FirstRegSaved =0, SecondRegSaved=0;
//  unsigned FirstRegSavedTo = 0, SecondRegSavedTo = 0;
//
//  Reg = Available.find_first();
//
//  if (Reg == -1) {
//    Reg = Candidates.find_first();
//    Candidates.reset(Reg);
//    if (DefReg != Reg) {
//      FirstRegSaved = Reg;
//      FirstRegSavedTo = LC2200::T0;
//      copyPhysReg(MBB, II, DL, FirstRegSavedTo, FirstRegSaved, true);
//    }
//  }
//  else
//    Available.reset(Reg);
//  BuildMI(MBB, II, DL, get(LC2200::LwConstant32), Reg).addImm(Imm).addImm(-1);
//  NewImm = 0;
//  if (FrameReg == LC2200::SP) {
//    SpReg = Available.find_first();
//    if (SpReg == -1) {
//      SpReg = Candidates.find_first();
//      // Candidates.reset(SpReg); // not really needed
//      if (DefReg!= SpReg) {
//        SecondRegSaved = SpReg;
//        SecondRegSavedTo = LC2200::T1;
//      }
//      if (SecondRegSaved)
//        copyPhysReg(MBB, II, DL, SecondRegSavedTo, SecondRegSaved, true);
//    }
//    else
//      Available.reset(SpReg);
//    copyPhysReg(MBB, II, DL, SpReg, LC2200::SP, false);
//    BuildMI(MBB, II, DL, get(LC2200::  AdduRxRyRz16), Reg).addReg(SpReg, RegState::Kill)
//            .addReg(Reg);
//  }
//  else
//    BuildMI(MBB, II, DL, get(LC2200::  AdduRxRyRz16), Reg).addReg(FrameReg)
//            .addReg(Reg, RegState::Kill);
//  if (FirstRegSaved || SecondRegSaved) {
//    II = std::next(II);
//    if (FirstRegSaved)
//      copyPhysReg(MBB, II, DL, FirstRegSaved, FirstRegSavedTo, true);
//    if (SecondRegSaved)
//      copyPhysReg(MBB, II, DL, SecondRegSaved, SecondRegSavedTo, true);
//  }
//  return Reg;
}

bool LC2200InstrInfo::validImmediate(unsigned Opcode, unsigned Reg, int64_t Amount) {
  return isInt<20>(Amount);
}

bool LC2200InstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch(Opcode) {
    default:
      break;
    case LC2200::ADDI:
      return (MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == LC2200::zero);
  }
  return MI.isAsCheapAsAMove();
}
