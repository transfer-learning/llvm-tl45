//===-- TL45Operators.td - TL45-specific operators ------*- tblgen-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

def TL45imm16 : Operand<i32>, ImmLeaf<i32, [{return isInt<16>(Imm);}]> {
  let EncoderMethod = "getImmOpValue";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<16>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def TL45Uimm16 : Operand<i32>, ImmLeaf<i32, [{return isUInt<16>(Imm);}]> {
  let EncoderMethod = "getImmOpValue";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<16>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def TL45imm32 : Operand<i32>, ImmLeaf<i32, [{return isInt<32>(Imm);}]> {
  let EncoderMethod = "getImmOpValue";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<32>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def jmp_target_h : Operand<OtherVT> {
    let EncoderMethod = "getJmpTargetHOpValue";
}

def jmp_target_l : Operand<OtherVT> {
    let EncoderMethod = "getJmpTargetLOpValue";
}


def jmptarget : Operand<OtherVT> {
  let EncoderMethod = "getJumpTargetOpValue";
  // let ParserMatchClass = MipsJumpTargetAsmOperand;
}

//===----------------------------------------------------------------------===//
// Operand Definitions.
//===----------------------------------------------------------------------===//

def b_target : Operand<OtherVT>;

def cc_val : Operand<i32>, ImmLeaf<i32, [{}]> {
//   let PrintMethod = "printCondCode";
}

//===----------------------------------------------------------------------===//
// Complex Pattern Definitions.
//===----------------------------------------------------------------------===//

// def addr : ComplexPattern<iPTR, 4, "SelectAddr", [], []>;