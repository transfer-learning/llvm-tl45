//===- TL45CallingConv.td - Calling Conventions for TL45 -*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This describes the calling conventions for TL45 architecture.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// TL45 Return Value Calling Convention
//===----------------------------------------------------------------------===//
def RetCC_TL45 : CallingConv<[
  // i32 are returned in registers v0
  CCIfType<[i8], CCPromoteToType<i32>>,
  CCIfType<[i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[r1, r2, r3, r4, r5]>>,
  CCIfType<[i32], CCAssignToStack<4,4>>
]>;

//===----------------------------------------------------------------------===//
// TL45 Argument Calling Conventions
//===----------------------------------------------------------------------===//
def CC_TL45 : CallingConv<[
  CCIfByVal<CCPassByVal<4,4>>,
  CCIfType<[i8], CCPromoteToType<i32>>,
  CCIfType<[i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[r6, r7, r8, r9]>>,
  CCIfType<[i32], CCAssignToStack<4,4>>,
  CCIfVarArg<CCAssignToStack<4,4>>
]>;

def CC_Save : CalleeSavedRegs<(add r10, r11, r12, r13, bp)>;
