
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "TL45ISelLowering.h"
#include "TL45TargetMachine.h"
#include <TL45MachineFunctionInfo.h>
#include <iostream>

#define DEBUG_TYPE "tl45-target-lowering"

using namespace llvm;

#include "TL45GenCallingConv.inc"

TL45TargetLowering::TL45TargetLowering(const TL45TargetMachine &TM,
                                       const TL45Subtarget &STI)
        : TargetLowering(TM), Subtarget(STI) {

    // Set up the register classes.
    addRegisterClass(MVT::i32, &TL45::GRRegsRegClass);

    // Compute derived properties from the register classes.
    computeRegisterProperties(STI.getRegisterInfo());


    // Tell LLVM about SP
    setStackPointerRegisterToSaveRestore(TL45::sp);

    for (auto N : {ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD})
        setLoadExtAction(N, MVT::i32, MVT::i1, Promote);

    setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i16, Expand);
    setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i16, Expand);

    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);

    // Variable Array (Dynamic StackAlloc)
    setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Expand);

    setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);

    setOperationAction(ISD::BRCOND, MVT::i32, Expand);

    // Rotate Expand
    setOperationAction(ISD::ROTL, MVT::i32, Expand);
    setOperationAction(ISD::ROTR, MVT::i32, Expand);

    setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
    setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);

    setOperationAction(ISD::SDIV, MVT::i32, Expand);
    setOperationAction(ISD::SREM, MVT::i32, Expand);

    setOperationAction(ISD::MULHS, MVT::i32, Expand);
    setOperationAction(ISD::MULHU, MVT::i32, Expand);

    setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
    setOperationAction(ISD::UDIVREM, MVT::i32, Expand);
    setOperationAction(ISD::UREM, MVT::i32, Expand);

    setOperationAction(ISD::BR_CC, MVT::Other, Custom);
    setOperationAction(ISD::BR_CC, MVT::i32, Custom);

    setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);
    setOperationAction(ISD::SELECT, MVT::i32, Custom);

    setOperationAction(ISD::BasicBlock, MVT::Other, Expand);

    setOperationAction(ISD::VASTART, MVT::Other, Custom);
    setOperationAction(ISD::VAARG, MVT::Other, Expand);
    setOperationAction(ISD::VACOPY, MVT::Other, Expand);
    setOperationAction(ISD::VAEND, MVT::Other, Expand);

    setOperationAction(ISD::SETCC, MVT::i32, Expand);
    setOperationAction(ISD::SETCC, MVT::Other, Expand);

    setOperationAction(ISD::BRCOND, MVT::i32, Expand);
    setOperationAction(ISD::BRCOND, MVT::Other, Expand);

    unsigned int Ops[] = {ISD::BSWAP, ISD::CTTZ, ISD::CTLZ, ISD::CTPOP, ISD::BITREVERSE};
    for (unsigned int Op : Ops)
        setOperationAction(Op, MVT::i32, Expand);

}

SDValue TL45TargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
    default:
      report_fatal_error("unimplemented operand");
    case ISD::BR_CC:
      return lowerBrCc(Op, DAG);
//  case ISD::BR:
//    return lowerBr(Op, DAG);
    case ISD::SELECT:
      return lowerSELECT(Op, DAG);
    case ISD::SELECT_CC:
      return lowerSelectCc(Op, DAG);
    case ISD::AND:
      return lowerAnd(Op, DAG);
    case ISD::OR:
      return lowerOr(Op, DAG);
    case ISD::XOR:
      return lowerXor(Op, DAG);
    case ISD::VASTART:
      return lowerVASTART(Op, DAG);
    case ISD::GlobalAddress:
      return lowerGlobalAddress(Op, DAG);
  }
}

void TL45TargetLowering::analyzeInputArgs(
        MachineFunction &MF, CCState &CCInfo,
        const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet) const {
    unsigned NumArgs = Ins.size();

    for (unsigned i = 0; i != NumArgs; ++i) {
        MVT ArgVT = Ins[i].VT;
        ISD::ArgFlagsTy ArgFlags = Ins[i].Flags;

        if (IsRet) {
            // caller interpreting returned values
            if (RetCC_TL45(i, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo)) {
                LLVM_DEBUG(dbgs() << "InputArg #" << i << " has unhandled type "
                                  << EVT(ArgVT).getEVTString() << '\n');
                llvm_unreachable(nullptr);
            }
        } else {
            // callee interpreting input args
            if (CC_TL45(i, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo)) {
                LLVM_DEBUG(dbgs() << "InputArg #" << i << " has unhandled type "
                                  << EVT(ArgVT).getEVTString() << '\n');
                llvm_unreachable(nullptr);
            }
        }
    }
}

void TL45TargetLowering::analyzeOutputArgs(
        MachineFunction &MF, CCState &CCInfo,
        const SmallVectorImpl<ISD::OutputArg> &Outs, bool IsRet,
        CallLoweringInfo *CLI) const {
    unsigned NumArgs = Outs.size();

    for (unsigned i = 0; i != NumArgs; i++) {
        MVT ArgVT = Outs[i].VT;
        ISD::ArgFlagsTy ArgFlags = Outs[i].Flags;
        // Type *OrigTy = CLI ? CLI->getArgs()[Outs[i].OrigArgIndex].Ty : nullptr;

        if (IsRet) {
            // callee emitting return values
            if (RetCC_TL45(i, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo)) {
                LLVM_DEBUG(dbgs() << "OutputArg #" << i << " has unhandled type "
                                  << EVT(ArgVT).getEVTString() << "\n");
                llvm_unreachable(nullptr);
            }
        } else {
            // caller emitting args
            if (CC_TL45(i, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo)) {
                LLVM_DEBUG(dbgs() << "OutputArg #" << i << " has unhandled type "
                                  << EVT(ArgVT).getEVTString() << "\n");
                llvm_unreachable(nullptr);
            }
        }
    }
}

// Convert Val to a ValVT. Should not be called for CCValAssign::Indirect
// values.
static SDValue convertLocVTToValVT(SelectionDAG &DAG, SDValue Val,
                                   const CCValAssign &VA, const SDLoc &DL) {
    switch (VA.getLocInfo()) {
        default:
            llvm_unreachable("Unexpected CCValAssign::LocInfo");
        case CCValAssign::Full:
            break;
        case CCValAssign::BCvt:
            //      if (VA.getLocVT() == MVT::i64 && VA.getValVT() == MVT::f32) {
            //        Val = DAG.getNode(RISCVISD::FMV_W_X_RV64, DL, MVT::f32, Val);
            //        break;
            //      }
            Val = DAG.getNode(ISD::BITCAST, DL, VA.getValVT(), Val);
            break;
    }
    return Val;
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromRegLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL) {
    MachineFunction &MF = DAG.getMachineFunction();
    MachineRegisterInfo &RegInfo = MF.getRegInfo();
    EVT LocVT = VA.getLocVT();
    SDValue Val;
    const TargetRegisterClass *RC;

    switch (LocVT.getSimpleVT().SimpleTy) {
        default:
            llvm_unreachable("Unexpected register type");
        case MVT::i32:
            RC = &TL45::GRRegsRegClass;
            break;
    }

    Register VReg = RegInfo.createVirtualRegister(RC);
    RegInfo.addLiveIn(VA.getLocReg(), VReg);
    Val = DAG.getCopyFromReg(Chain, DL, VReg, LocVT);

    if (VA.getLocInfo() == CCValAssign::Indirect)
        return Val;

    return convertLocVTToValVT(DAG, Val, VA, DL);
}

static SDValue convertValVTToLocVT(SelectionDAG &DAG, SDValue Val,
                                   const CCValAssign &VA, const SDLoc &DL) {
    EVT LocVT = VA.getLocVT();

    switch (VA.getLocInfo()) {
        default:
            llvm_unreachable("Unexpected CCValAssign::LocInfo");
        case CCValAssign::Full:
            break;
        case CCValAssign::BCvt:
            //      if (VA.getLocVT() == MVT::i64 && VA.getValVT() == MVT::f32) {
            //        Val = DAG.getNode(RISCVISD::FMV_X_ANYEXTW_RV64, DL, MVT::i64,
            //        Val); break;
            //      }
            Val = DAG.getNode(ISD::BITCAST, DL, LocVT, Val);
            break;
    }
    return Val;
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromMemLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL) {
    MachineFunction &MF = DAG.getMachineFunction();
    MachineFrameInfo &MFI = MF.getFrameInfo();
    EVT LocVT = VA.getLocVT();
    EVT ValVT = VA.getValVT();
    EVT PtrVT = MVT::getIntegerVT(DAG.getDataLayout().getPointerSizeInBits(0));
    // TODO: check: -1 adjustment is for return address
    uint64_t SlotSize = ValVT.getSizeInBits() / 8;
    int FI = MFI.CreateFixedObject(SlotSize,
                                   VA.getLocMemOffset(), /*IsImmutable=*/true);
    SDValue FIN = DAG.getFrameIndex(FI, PtrVT);
    SDValue Val;

    ISD::LoadExtType ExtType;
    switch (VA.getLocInfo()) {
        default:
            llvm_unreachable("Unexpected CCValAssign::LocInfo");
        case CCValAssign::Full:
        case CCValAssign::Indirect:
        case CCValAssign::BCvt:
            ExtType = ISD::NON_EXTLOAD;
            break;
    }
    Val = DAG.getExtLoad(
            ExtType, DL, LocVT, Chain, FIN,
            MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI), ValVT);
    return Val;
}

static const MCPhysReg ArgGPRs[] = {
        TL45::r2, TL45::r3, TL45::r4, TL45::r5, TL45::r6
};

// Transform physical registers into virtual registers.
SDValue TL45TargetLowering::LowerFormalArguments(
        SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
        const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
        SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

    switch (CallConv) {
        case CallingConv::C:
        case CallingConv::Fast:
            break;
        default:
            report_fatal_error("Unsupported calling convention");
    }

    MachineFunction &MF = DAG.getMachineFunction();

    const Function &Func = MF.getFunction();
    if (Func.hasFnAttribute("interrupt")) {
        if (!Func.arg_empty())
            report_fatal_error(
                    "Functions with the interrupt attribute cannot have arguments!");

        StringRef Kind =
                MF.getFunction().getFnAttribute("interrupt").getValueAsString();

        if (!(Kind == "user" || Kind == "supervisor" || Kind == "machine"))
            report_fatal_error(
                    "Function interrupt attribute argument not supported!");
    }

    EVT PtrVT = getPointerTy(DAG.getDataLayout());
    unsigned int PtrLengthInBytes = PtrVT.getSizeInBits() / 8;
    MVT XLenVT = MVT::i32;
    unsigned XLenInBytes = XLenVT.getSizeInBits() / 8;
    // Used with vargs to acumulate store chains.
    std::vector<SDValue> OutChains;

    // Assign locations to all of the incoming arguments.
    SmallVector<CCValAssign, 16> ArgLocs;
    CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
    CCInfo.AllocateStack(PtrLengthInBytes, PtrLengthInBytes);
    analyzeInputArgs(MF, CCInfo, Ins, /*IsRet=*/false);

    for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
        CCValAssign &VA = ArgLocs[i];
        SDValue ArgValue;
        if (VA.isRegLoc())
            ArgValue = unpackFromRegLoc(DAG, Chain, VA, DL);
        else
            ArgValue = unpackFromMemLoc(DAG, Chain, VA, DL);

        if (VA.getLocInfo() == CCValAssign::Indirect) {
            // If the original argument was split and passed by reference (e.g. i128
            // on RV32), we need to load all parts of it here (using the same
            // address).
            InVals.push_back(DAG.getLoad(VA.getValVT(), DL, Chain, ArgValue,
                                         MachinePointerInfo()));
            unsigned ArgIndex = Ins[i].OrigArgIndex;
            assert(Ins[i].PartOffset == 0);
            while (i + 1 != e && Ins[i + 1].OrigArgIndex == ArgIndex) {
                CCValAssign &PartVA = ArgLocs[i + 1];
                unsigned PartOffset = Ins[i + 1].PartOffset;
                SDValue Address = DAG.getNode(ISD::ADD, DL, PtrVT, ArgValue,
                                              DAG.getIntPtrConstant(PartOffset, DL));
                InVals.push_back(DAG.getLoad(PartVA.getValVT(), DL, Chain, Address,
                                             MachinePointerInfo()));
                ++i;
            }
            continue;
        }
        InVals.push_back(ArgValue);
    }

    if (IsVarArg) {
        ArrayRef<MCPhysReg> ArgRegs = makeArrayRef(ArgGPRs);
        unsigned Idx = CCInfo.getFirstUnallocated(ArgRegs);
        const TargetRegisterClass *RC = &TL45::GRRegsRegClass;
        MachineFrameInfo &MFI = MF.getFrameInfo();
        MachineRegisterInfo &RegInfo = MF.getRegInfo();
        TL45MachineFunctionInfo *TL45MFI = MF.getInfo<TL45MachineFunctionInfo>();

        // Offset of the first variable argument from stack pointer, and size of
        // the vararg save area. For now, the varargs save area is either zero or
        // large enough to hold a0-a7.
        int VaArgOffset, VarArgsSaveSize;

        // If all registers are allocated, then all varargs must be passed on the
        // stack and we don't need to save any argregs.
//    if (ArgRegs.size() == Idx) {
        VaArgOffset = CCInfo.getNextStackOffset();
        VarArgsSaveSize = 0;
//    } else {
//      VarArgsSaveSize = XLenInBytes * (ArgRegs.size() - Idx);
//      VaArgOffset = -VarArgsSaveSize;
//    }

        // Record the frame index of the first variable argument
        // which is a value necessary to VASTART.
        int FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
        // Subtract RA slot
        uint64_t RASlotSize = PtrVT.getSizeInBits() / 8;
        TL45MFI->setVarArgsFrameIndex(FI);

        // Copy the integer registers that may have been used for passing varargs
        // to the vararg save area.
//    for (unsigned I = Idx; I < ArgRegs.size();
//         ++I, VaArgOffset += XLenInBytes) {
//      const Register Reg = RegInfo.createVirtualRegister(RC);
//      RegInfo.addLiveIn(ArgRegs[I], Reg);
//      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, XLenVT);
//      FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
//      SDValue PtrOff = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
//      SDValue Store = DAG.getStore(Chain, DL, ArgValue, PtrOff,
//                                   MachinePointerInfo::getFixedStack(MF, FI));
//      cast<StoreSDNode>(Store.getNode())
//          ->getMemOperand()
//          ->setValue((Value *)nullptr);
//      OutChains.push_back(Store);
//    }
        TL45MFI->setVarArgsSaveSize(VarArgsSaveSize);
    }

    // All stores are grouped in one node to allow the matching between
    // the size of Ins and InVals. This only happens for vararg functions.
    if (!OutChains.empty()) {
        OutChains.push_back(Chain);
        Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
    }

    return Chain;
}

SDValue TL45TargetLowering::lowerVASTART(SDValue Op, SelectionDAG &DAG) const {
    MachineFunction &MF = DAG.getMachineFunction();
    TL45MachineFunctionInfo *FuncInfo = MF.getInfo<TL45MachineFunctionInfo>();

    SDLoc DL(Op);
    SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                   getPointerTy(MF.getDataLayout()));

    // vastart just stores the address of the VarArgsFrameIndex slot into the
    // memory location argument.
    const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
    return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                        MachinePointerInfo(SV));
}

SDValue
TL45TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool IsVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &DL, SelectionDAG &DAG) const {
    // Stores the assignment of the return value to a location.
    SmallVector<CCValAssign, 16> RVLocs;

    // Info about the registers and stack slot.
    CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                   *DAG.getContext());

    analyzeOutputArgs(DAG.getMachineFunction(), CCInfo, Outs, /*IsRet=*/true,
                      nullptr);

    SDValue Glue;
    SmallVector<SDValue, 4> RetOps(1, Chain);

    // Copy the result values into the output registers.
    for (unsigned i = 0, e = RVLocs.size(); i < e; ++i) {
        SDValue Val = OutVals[i];
        CCValAssign &VA = RVLocs[i];
        assert(VA.isRegLoc() && "Can only return in registers!");

        if (VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64) {
            // Handle returning f64 on RV32D with a soft float ABI.
            llvm_unreachable("not supported");
            //      assert(VA.isRegLoc() && "Expected return via registers");
            //      SDValue SplitF64 = DAG.getNode(RISCVISD::SplitF64, DL,
            //                                     DAG.getVTList(MVT::i32, MVT::i32),
            //                                     Val);
            //      SDValue Lo = SplitF64.getValue(0);
            //      SDValue Hi = SplitF64.getValue(1);
            //      Register RegLo = VA.getLocReg();
            //      Register RegHi = RegLo + 1;
            //      Chain = DAG.getCopyToReg(Chain, DL, RegLo, Lo, Glue);
            //      Glue = Chain.getValue(1);
            //      RetOps.push_back(DAG.getRegister(RegLo, MVT::i32));
            //      Chain = DAG.getCopyToReg(Chain, DL, RegHi, Hi, Glue);
            //      Glue = Chain.getValue(1);
            //      RetOps.push_back(DAG.getRegister(RegHi, MVT::i32));
        } else {
            // Handle a 'normal' return.
            Val = convertValVTToLocVT(DAG, Val, VA, DL);
            Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);

            // Guarantee that all emitted copies are stuck together.
            Glue = Chain.getValue(1);
            RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
        }
    }

    RetOps[0] = Chain; // Update chain.

    // Add the glue node if we have it.
    if (Glue.getNode()) {
        RetOps.push_back(Glue);
    }

    // Interrupt service routines use different return instructions.
    const Function &Func = DAG.getMachineFunction().getFunction();
    if (Func.hasFnAttribute("interrupt")) {
        llvm_unreachable("interrupts not supported");
        //    if (!Func.getReturnType()->isVoidTy())
        //      report_fatal_error(
        //              "Functions with the interrupt attribute must have void
        //              return type!");
        //
        //    MachineFunction &MF = DAG.getMachineFunction();
        //    StringRef Kind =
        //            MF.getFunction().getFnAttribute("interrupt").getValueAsString();
        //
        //    unsigned RetOpc;
        //    if (Kind == "user")
        //      RetOpc = RISCVISD::URET_FLAG;
        //    else if (Kind == "supervisor")
        //      RetOpc = RISCVISD::SRET_FLAG;
        //    else
        //      RetOpc = RISCVISD::MRET_FLAG;
        //
        //    return DAG.getNode(RetOpc, DL, MVT::Other, RetOps);
    }

    return DAG.getNode(TL45ISD::RET, DL, MVT::Other, RetOps);
}

/* CALL */
SDValue
TL45TargetLowering::LowerCall(CallLoweringInfo &CLI,
                              SmallVectorImpl<SDValue> &InVals) const {
    SelectionDAG &DAG = CLI.DAG;
    SDLoc &DL = CLI.DL;
    SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
    SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
    SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
    SDValue Chain = CLI.Chain;
    SDValue Callee = CLI.Callee;
    bool &IsTailCall = CLI.IsTailCall;
    CallingConv::ID CallConv = CLI.CallConv;
    bool IsVarArg = CLI.IsVarArg;
    EVT PtrVT = getPointerTy(DAG.getDataLayout());
    MVT XLenVT = MVT::i32;

    MachineFunction &MF = DAG.getMachineFunction();

    // Analyze the operands of the call, assigning locations to each operand.
    SmallVector<CCValAssign, 16> ArgLocs;
    CCState ArgCCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
    analyzeOutputArgs(MF, ArgCCInfo, Outs, /*IsRet=*/false, &CLI);

    IsTailCall = false;
    // Check if it's really possible to do a tail call.
    //  if (IsTailCall)
    //    IsTailCall = isEligibleForTailCallOptimization(ArgCCInfo, CLI, MF,
    //    ArgLocs);
    //
    //  if (IsTailCall)
    //    ++NumTailCalls;
    //  else
    if (CLI.CS && CLI.CS.isMustTailCall())
        report_fatal_error("failed to perform tail call elimination on a call "
                           "site marked musttail");

    // Get a count of how many bytes are to be pushed on the stack.
    unsigned NumBytes = ArgCCInfo.getNextStackOffset();

    // Create local copies for byval args
    SmallVector<SDValue, 8> ByValArgs;
    for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
        ISD::ArgFlagsTy Flags = Outs[i].Flags;
        if (!Flags.isByVal())
            continue;

        SDValue Arg = OutVals[i];
        unsigned Size = Flags.getByValSize();
        unsigned Align = Flags.getByValAlign();

        int FI = MF.getFrameInfo().CreateStackObject(Size, Align, /*isSS=*/false);
        SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
        SDValue SizeNode = DAG.getConstant(Size, DL, XLenVT);

        Chain = DAG.getMemcpy(Chain, DL, FIPtr, Arg, SizeNode, Align,
                /*IsVolatile=*/false,
                /*AlwaysInline=*/false, IsTailCall,
                              MachinePointerInfo(), MachinePointerInfo());
        ByValArgs.push_back(FIPtr);
    }

    if (!IsTailCall)
        Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, CLI.DL);

    // Copy argument values to their designated locations.
    SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
    SmallVector<SDValue, 8> MemOpChains;
    SDValue StackPtr;
    for (unsigned i = 0, j = 0, e = ArgLocs.size(); i != e; ++i) {
        CCValAssign &VA = ArgLocs[i];
        SDValue ArgValue = OutVals[i];
        ISD::ArgFlagsTy Flags = Outs[i].Flags;

        // IsF64OnRV32DSoftABI && VA.isMemLoc() is handled below in the same way
        // as any other MemLoc.

        // Promote the value if needed.
        // For now, only handle fully promoted and indirect arguments.
        if (VA.getLocInfo() == CCValAssign::Indirect) {
            // Store the argument in a stack slot and pass its address.
            SDValue SpillSlot = DAG.CreateStackTemporary(Outs[i].ArgVT);
            int FI = cast<FrameIndexSDNode>(SpillSlot)->getIndex();
            MemOpChains.push_back(
                    DAG.getStore(Chain, DL, ArgValue, SpillSlot,
                                 MachinePointerInfo::getFixedStack(MF, FI)));
            // If the original argument was split (e.g. i128), we need
            // to store all parts of it here (and pass just one address).
            unsigned ArgIndex = Outs[i].OrigArgIndex;
            assert(Outs[i].PartOffset == 0);
            while (i + 1 != e && Outs[i + 1].OrigArgIndex == ArgIndex) {
                SDValue PartValue = OutVals[i + 1];
                unsigned PartOffset = Outs[i + 1].PartOffset;
                SDValue Address = DAG.getNode(ISD::ADD, DL, PtrVT, SpillSlot,
                                              DAG.getIntPtrConstant(PartOffset, DL));
                MemOpChains.push_back(
                        DAG.getStore(Chain, DL, PartValue, Address,
                                     MachinePointerInfo::getFixedStack(MF, FI)));
                ++i;
            }
            ArgValue = SpillSlot;
        } else {
            ArgValue = convertValVTToLocVT(DAG, ArgValue, VA, DL);
        }

        // Use local copy if it is a byval arg.
        if (Flags.isByVal())
            ArgValue = ByValArgs[j++];

        if (VA.isRegLoc()) {
            // Queue up the argument copies and emit them at the end.
            RegsToPass.push_back(std::make_pair(VA.getLocReg(), ArgValue));
        } else {
            assert(VA.isMemLoc() && "Argument not register or memory");
            assert(!IsTailCall && "Tail call not allowed if stack is used "
                                  "for passing parameters");

            // Work out the address of the stack slot.
            if (!StackPtr.getNode())
                StackPtr = DAG.getCopyFromReg(Chain, DL, TL45::sp, PtrVT);
            SDValue Address =
                    DAG.getNode(ISD::ADD, DL, PtrVT, StackPtr,
                                DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));

            // Emit the store.
            MemOpChains.push_back(
                    DAG.getStore(Chain, DL, ArgValue, Address, MachinePointerInfo()));
        }
    }

    // Join the stores, which are independent of one another.
    if (!MemOpChains.empty())
        Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

    SDValue Glue;

    // Build a sequence of copy-to-reg nodes, chained and glued together.
    for (auto &Reg : RegsToPass) {
        Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, Glue);
        Glue = Chain.getValue(1);
    }

    // If the callee is a GlobalAddress/ExternalSymbol node, turn it into a
    // TargetGlobalAddress/TargetExternalSymbol node so that legalize won't
    // split it and then direct call can be matched by PseudoCALL.
    if (GlobalAddressSDNode *S = dyn_cast<GlobalAddressSDNode>(Callee)) {
        const GlobalValue *GV = S->getGlobal();

        unsigned OpFlags = 0; // RISCVII::MO_CALL;
        //    if (!getTargetMachine().shouldAssumeDSOLocal(*GV->getParent(), GV))
        //      OpFlags = RISCVII::MO_PLT;

        Callee = DAG.getTargetGlobalAddress(GV, DL, PtrVT, 0, OpFlags);
    } else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
        unsigned OpFlags = 0; // RISCVII::MO_CALL;
        //
        //    if
        //    (!getTargetMachine().shouldAssumeDSOLocal(*MF.getFunction().getParent(),
        //                                                 nullptr))
        //      OpFlags = RISCVII::MO_PLT;

        Callee = DAG.getTargetExternalSymbol(S->getSymbol(), PtrVT, OpFlags);
    }

    // The first call operand is the chain and the second is the target address.
    SmallVector<SDValue, 8> Ops;
    Ops.push_back(Chain);
    Ops.push_back(Callee);

    // Add argument registers to the end of the list so that they are
    // known live into the call.
    for (auto &Reg : RegsToPass)
        Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

    if (!IsTailCall) {
        // Add a register mask operand representing the call-preserved registers.
        const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
        const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
        assert(Mask && "Missing call preserved mask for calling convention");
        Ops.push_back(DAG.getRegisterMask(Mask));
    }

    // Glue the call to the argument copies, if any.
    if (Glue.getNode())
        Ops.push_back(Glue);

    // Emit the call.
    SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);

    //  if (IsTailCall) {
    //    MF.getFrameInfo().setHasTailCall();
    //    return DAG.getNode(RISCVISD::TAIL, DL, NodeTys, Ops);
    //  }

    Chain = DAG.getNode(TL45ISD::CALL, DL, NodeTys, Ops);
    Glue = Chain.getValue(1);
    // ACTUAL CALL INSTRUCTION


    // Mark the end of the call, which is glued to the call itself.
    Chain = DAG.getCALLSEQ_END(Chain, DAG.getConstant(NumBytes, DL, PtrVT, true),
                               DAG.getConstant(0, DL, PtrVT, true), Glue, DL);
    Glue = Chain.getValue(1);

    // Assign locations to each value returned by this call.
    SmallVector<CCValAssign, 16> RVLocs;
    CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
    analyzeInputArgs(MF, RetCCInfo, Ins, /*IsRet=*/true);

    // Copy all of the result registers out of their specified physreg.
    for (auto &VA : RVLocs) {
        // Copy the value out
        SDValue RetValue =
                DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
        // Glue the RetValue to the end of the call sequence
        Chain = RetValue.getValue(1);
        Glue = RetValue.getValue(2);

        RetValue = convertLocVTToValVT(DAG, RetValue, VA, DL);

        InVals.push_back(RetValue);
    }

    return Chain;
}

static SDValue getTargetNode(GlobalAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
    return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, 0, Flags);
}

static SDValue getTargetNode(BlockAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
    return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, N->getOffset(),
                                     Flags);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
    return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlignment(),
                                     N->getOffset(), Flags);
}

template<class NodeTy>
SDValue TL45TargetLowering::getAddr(NodeTy *N, SelectionDAG &DAG,
                                    bool IsLocal) const {
    SDLoc DL(N);
    EVT Ty = getPointerTy(DAG.getDataLayout());
    SDValue r0 = DAG.getRegister(TL45::r0, MVT::i32);

    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    return SDValue(DAG.getMachineNode(TL45::ADDI, DL, Ty, r0, Addr), 0);
}

SDValue TL45TargetLowering::lowerGlobalAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
    SDLoc DL(Op);
    EVT Ty = Op.getValueType();
    GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
    int64_t Offset = N->getOffset();
    MVT XLenVT = MVT::i32;

    const GlobalValue *GV = N->getGlobal();
    bool IsLocal = getTargetMachine().shouldAssumeDSOLocal(*GV->getParent(), GV);
    SDValue Addr = getAddr(N, DAG, IsLocal);

    // In order to maximise the opportunity for common subexpression elimination,
    // emit a separate ADD node for the global address offset instead of folding
    // it in the global address node. Later peephole optimisations may choose to
    // fold it back in when profitable.

    SDValue root = DAG.getNode(ISD::ADD, DL, MVT::i32, Addr, DAG.getConstant(Offset, DL, MVT::i32));

    return root;
}

SDValue TL45TargetLowering::ExpandLibCall(const char *LibcallName, SDValue Op, bool isSigned, SelectionDAG &DAG) const {
    SDNode *Node = Op.getNode();

    TargetLowering::ArgListTy Args;
    TargetLowering::ArgListEntry Entry;
    for (const SDValue &Op : Node->op_values()) {
        EVT ArgVT = Op.getValueType();
        Type *ArgTy = ArgVT.getTypeForEVT(*DAG.getContext());
        Entry.Node = Op;
        Entry.Ty = ArgTy;
        Entry.IsSExt = shouldSignExtendTypeInLibCall(ArgVT, isSigned);
        Entry.IsZExt = !shouldSignExtendTypeInLibCall(ArgVT, isSigned);
        Args.push_back(Entry);
    }
    SDValue Callee = DAG.getExternalSymbol(LibcallName, getPointerTy(DAG.getDataLayout()));

    EVT RetVT = Node->getValueType(0);
    Type *RetTy = RetVT.getTypeForEVT(*DAG.getContext());

    // By default, the input chain to this libcall is the entry node of the
    // function. If the libcall is going to be emitted as a tail call then
    // TLI.isUsedByReturnOnly will change it to the right chain if the return
    // node which is being folded has a non-entry input chain.
    SDValue InChain = DAG.getEntryNode();

    // isTailCall may be true since the callee does not reference caller stack
    // frame. Check if it's in the right position and that the return types match.
    SDValue TCChain = InChain;
    const Function &F = DAG.getMachineFunction().getFunction();
    bool isTailCall =
            isInTailCallPosition(DAG, Node, TCChain) &&
            (RetTy == F.getReturnType() || F.getReturnType()->isVoidTy());
    if (isTailCall)
        InChain = TCChain;

    TargetLowering::CallLoweringInfo CLI(DAG);
    bool signExtend = shouldSignExtendTypeInLibCall(RetVT, isSigned);
    CLI.setDebugLoc(SDLoc(Node))
            .setChain(InChain)
            .setLibCallee(CallingConv::C, RetTy, Callee, std::move(Args))
            .setTailCall(isTailCall)
            .setSExtResult(signExtend)
            .setZExtResult(!signExtend)
            .setIsPostTypeLegalization(true);

    std::pair<SDValue, SDValue> CallInfo = LowerCallTo(CLI);

    if (!CallInfo.second.getNode()) {
//    LLVM_DEBUG(dbgs() << "Created tailcall: "; DAG.getRoot().dump(&DAG));
        // It's a tailcall, return the chain (which is the DAG root).
        return DAG.getRoot();
    }

//  LLVM_DEBUG(dbgs() << "Created libcall: "; CallInfo.first.dump(&DAG));
    return CallInfo.first;
}

SDValue TL45TargetLowering::lowerShiftLeft(SDValue Op, SelectionDAG &DAG) const {
    SDLoc DL(Op);
    SDValue Vl = Op.getOperand(0);
    SDValue ShiftAmt = Op.getOperand(1);

    if (ShiftAmt.getOpcode() != ISD::Constant) {
        return ExpandLibCall("__shlu", Op, false, DAG);
    }

    // fold constant shift into repeated ADDs

    EVT VT = Vl.getValueType();
    uint64_t ShAmt = cast<ConstantSDNode>(ShiftAmt.getNode())->getZExtValue();

    for (uint64_t i = 0; i < ShAmt; i++) {
        Vl = DAG.getNode(ISD::ADD, DL, VT, Vl, Vl);
    }

    return Vl;
}

SDValue TL45TargetLowering::lowerBr(SDValue Op, SelectionDAG &DAG) const {
    SDLoc DagLoc(Op);
    SDValue Chain = Op.getOperand(0);
    SDValue Dest = Op.getOperand(1);
    SDLoc DL(Op);

    SDValue Jmp = DAG.getNode(TL45ISD::JMP, DL, MVT::Other, Chain, Dest);
    return Jmp;
}

SDValue TL45TargetLowering::lowerBrCc(SDValue Op, SelectionDAG &DAG) const {
    SDLoc DagLoc(Op);
    SDValue Chain = Op.getOperand(0);
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
    SDValue LHS = Op.getOperand(2);
    SDValue RHS = Op.getOperand(3);
    SDValue Dest = Op.getOperand(4);

    SDLoc DL(Op);

    if (isa<ConstantSDNode>(RHS) && cast<ConstantSDNode>(RHS)->getConstantIntValue()->getValue().isSignedIntN(16)) {
        return DAG.getNode(TL45ISD::CMPI_JMP, DL, MVT::Other, Chain,
                           DAG.getConstant(CC, DL, MVT::i32), LHS, RHS, Dest);
    } else if (isa<ConstantSDNode>(LHS) &&
               cast<ConstantSDNode>(LHS)->getConstantIntValue()->getValue().isSignedIntN(16)) {
        return DAG.getNode(TL45ISD::CMPI_JMP, DL, MVT::Other, Chain,
                           DAG.getConstant(ISD::getSetCCSwappedOperands(CC), DL, MVT::i32), RHS, LHS, Dest);
    } else {
        return DAG.getNode(TL45ISD::CMP_JMP, DL, MVT::Other, Chain,
                           DAG.getConstant(CC, DL, MVT::i32), LHS, RHS, Dest);
    }
}

SDValue TL45TargetLowering::lowerSELECT(SDValue Op, SelectionDAG &DAG) const {
    SDValue CondV = Op.getOperand(0);
    SDValue TrueV = Op.getOperand(1);
    SDValue FalseV = Op.getOperand(2);
    SDLoc DL(Op);
    MVT XLenVT = MVT::i32;

    // Otherwise:
    // (select condv, truev, falsev)
    // -> (riscvisd::select_cc condv, zero, setne, truev, falsev)
    SDValue Zero = DAG.getConstant(0, DL, XLenVT);
    SDValue SetNE = DAG.getConstant(ISD::SETNE, DL, XLenVT);

    SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Glue);
    SDValue Ops[] = {CondV, Zero, SetNE, TrueV, FalseV};

    return DAG.getNode(TL45ISD::SELECT_CC, DL, VTs, Ops);
}

SDValue TL45TargetLowering::lowerSelectCc(SDValue Op,
                                          SelectionDAG &DAG) const {
    SDLoc DagLoc(Op);
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);
    SDValue TrueValue = Op.getOperand(2);
    SDValue FalseValue = Op.getOperand(3);
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();

    SDLoc DL(Op);
    EVT TrueType = TrueValue.getValueType();
    EVT FalseType = FalseValue.getValueType();
    if (TrueType != FalseType) {
        llvm_unreachable("mismatched types of select_cc true and false nodes");
    }

    SDValue SelectMove = DAG.getNode(TL45ISD::SELECT_CC, DL, TrueType,
                                     LHS, RHS, DAG.getConstant(CC, DL, MVT::i32), TrueValue, FalseValue);
    return SelectMove;
}

static SDValue notValue(SelectionDAG &DAG, const SDLoc &DL, EVT VT, SDValue Value) {
    return DAG.getNode(TL45ISD::NAND, DL, VT, Value, Value);
}

SDValue TL45TargetLowering::lowerAnd(SDValue Op, SelectionDAG &DAG) const {
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);
    EVT VT = LHS.getValueType();
    SDLoc DL(Op);
    // A AND B = (A NAND B) NAND (A NAND B)
    SDValue Nand = DAG.getNode(TL45ISD::NAND, DL, VT, LHS, RHS);
    SDValue And = notValue(DAG, DL, VT, Nand);
    return And;
}

SDValue TL45TargetLowering::lowerOr(SDValue Op, SelectionDAG &DAG) const {
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);
    EVT VT = LHS.getValueType();
    SDLoc DL(Op);
    // A OR B = (NOT A) NAND (NOT B) = (A NAND A) NAND (B NAND B)
    SDValue NotA = notValue(DAG, DL, VT, LHS);
    SDValue NotB = notValue(DAG, DL, VT, RHS);
    SDValue Or = DAG.getNode(TL45ISD::NAND, DL, VT, NotA, NotB);
    return Or;
}

SDValue TL45TargetLowering::lowerXor(SDValue Op, SelectionDAG &DAG) const {
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);

    EVT VT = LHS.getValueType();
    SDLoc DL(Op);
    // A XOR B = ((((a NAND b) NAND a) NAND ((a NAND b) NAND b)))
    SDValue NandAB = DAG.getNode(TL45ISD::NAND, DL, VT, LHS, RHS);

    SDValue NandABNandA = DAG.getNode(TL45ISD::NAND, DL, VT, NandAB, LHS);
    SDValue NandABNandB = DAG.getNode(TL45ISD::NAND, DL, VT, NandAB, RHS);

    SDValue Xor = DAG.getNode(TL45ISD::NAND, DL, VT, NandABNandA, NandABNandB);
    return Xor;
}

std::pair<unsigned, const TargetRegisterClass *>
TL45TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                 StringRef Constraint,
                                                 MVT VT) const {
    // First, see if this is a constraint that directly corresponds to a
    // TL45 register class.
    if (Constraint.size() == 1) {
        switch (Constraint[0]) {
            case 'r':
                return std::make_pair(0U, &TL45::GRRegsRegClass);
            default:
                break;
        }
    }

    // Clang will correctly decode the usage of register name aliases into their
    // official names. However, other frontends like `rustc` do not. This allows
    // users of these frontends to use the ABI names for registers in LLVM-style
    // register constraints.
    Register XRegFromAlias = StringSwitch<Register>(Constraint.lower())
            .Cases("{zero}", "{r0}", TL45::r0)
            .Case("{r1}", TL45::r1)
            .Case("{r2}", TL45::r2)
            .Case("{r3}", TL45::r3)
            .Case("{r4}", TL45::r4)
            .Case("{r5}", TL45::r5)
            .Case("{r6}", TL45::r6)
            .Case("{r7}", TL45::r7)
            .Case("{r8}", TL45::r8)
            .Case("{r9}", TL45::r9)
            .Case("{r10}", TL45::r10)
            .Case("{r11}", TL45::r11)
            .Case("{r12}", TL45::r12)
            .Case("{r13}", TL45::r13)
            .Cases("{bp}", "{r14}", TL45::bp)
            .Cases("{sp}", "{r15}", TL45::sp)
            .Default(TL45::NoRegister);
    if (XRegFromAlias != TL45::NoRegister)
        return std::make_pair(XRegFromAlias, &TL45::GRRegsRegClass);

    return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

const char *TL45TargetLowering::getTargetNodeName(unsigned Opcode) const {
    switch ((TL45ISD::NodeType) Opcode) {
        case TL45ISD::FIRST_NUMBER:
            break;
        case TL45ISD::RET:
            return "TL45ISD::RET";
        case TL45ISD::CALL:
            return "TL45ISD::CALL";
        case TL45ISD::JMP:
            return "TL45ISD::JMP";
        case TL45ISD::NAND:
            return "TL45ISD::NAND";
        case TL45ISD::CMP_SKIP:
            return "TL45ISD::CMP_SKIP";
        case TL45ISD::CMP_JMP:
            return "TL45ISD::CMP_JMP";
        case TL45ISD::CMPI_JMP:
            return "TL45ISD::CMPI_JMP";
        case TL45ISD::SUB_TERM:
            return "TL45ISD::SUB_TERM";
        case TL45ISD::SELECT_MOVE:
            return "TL45ISD::SELECT_MOVE";
        case TL45ISD::CMP_SELECT_MOVE:
            return "TL45ISD::CMP_SELECT_MOVE";
    }
    return nullptr;
}

static bool isSelectPseudo(MachineInstr &MI) {
    switch (MI.getOpcode()) {
        default:
            return false;
        case TL45::Select_GRRegs_Using_CC_GRRegs:
            return true;
    }
}

// Return the RISC-V branch opcode that matches the given DAG integer
// condition code. The CondCode must be one of those supported by the RISC-V
// ISA (see normaliseSetCC).
static unsigned getBranchOpcodeForIntCondCode(ISD::CondCode CC) {
    switch (CC) {
        default:
            llvm_unreachable("Unsupported CondCode");
        case ISD::SETEQ:
            return TL45::JEI;
        case ISD::SETNE:
            return TL45::JNEI;
        case ISD::SETLT:
            return TL45::JLI;
        case ISD::SETGE:
            return TL45::JGI;
        case ISD::SETULT:
            return TL45::JBI;
        case ISD::SETUGE:
            return TL45::JNBI;
        case ISD::SETGT:
            return TL45::JGI;
        case ISD::SETUGT:
            return TL45::JAI;
        case ISD::SETLE:
            return TL45::JLEI;
        case ISD::SETULE:
            return TL45::JBEI;
    }
}

static MachineBasicBlock *emitSelectPseudo(MachineInstr &MI,
                                           MachineBasicBlock *BB) {
    // To "insert" Select_* instructions, we actually have to insert the triangle
    // control-flow pattern.  The incoming instructions know the destination vreg
    // to set, the condition code register to branch on, the true/false values to
    // select between, and the condcode to use to select the appropriate branch.
    //
    // We produce the following control flow:
    //     HeadMBB
    //     |  \
    //     |  IfFalseMBB
    //     | /
    //    TailMBB
    //
    // When we find a sequence of selects we attempt to optimize their emission
    // by sharing the control flow. Currently we only handle cases where we have
    // multiple selects with the exact same condition (same LHS, RHS and CC).
    // The selects may be interleaved with other instructions if the other
    // instructions meet some requirements we deem safe:
    // - They are debug instructions. Otherwise,
    // - They do not have side-effects, do not access memory and their inputs do
    //   not depend on the results of the select pseudo-instructions.
    // The TrueV/FalseV operands of the selects cannot depend on the result of
    // previous selects in the sequence.
    // These conditions could be further relaxed. See the X86 target for a
    // related approach and more information.
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    auto CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());

    SmallVector<MachineInstr *, 4> SelectDebugValues;
    SmallSet<Register, 4> SelectDests;
    SelectDests.insert(MI.getOperand(0).getReg());

    MachineInstr *LastSelectPseudo = &MI;

    for (auto E = BB->end(), SequenceMBBI = MachineBasicBlock::iterator(MI);
         SequenceMBBI != E; ++SequenceMBBI) {
        if (SequenceMBBI->isDebugInstr())
            continue;
        else if (isSelectPseudo(*SequenceMBBI)) {
            if (SequenceMBBI->getOperand(1).getReg() != LHS ||
                SequenceMBBI->getOperand(2).getReg() != RHS ||
                SequenceMBBI->getOperand(3).getImm() != CC ||
                SelectDests.count(SequenceMBBI->getOperand(4).getReg()) ||
                SelectDests.count(SequenceMBBI->getOperand(5).getReg()))
                break;
            LastSelectPseudo = &*SequenceMBBI;
            SequenceMBBI->collectDebugValues(SelectDebugValues);
            SelectDests.insert(SequenceMBBI->getOperand(0).getReg());
        } else {
            if (SequenceMBBI->hasUnmodeledSideEffects() ||
                SequenceMBBI->mayLoadOrStore())
                break;
            if (llvm::any_of(SequenceMBBI->operands(), [&](MachineOperand &MO) {
                return MO.isReg() && MO.isUse() && SelectDests.count(MO.getReg());
            }))
                break;
        }
    }

    const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
    const BasicBlock *LLVM_BB = BB->getBasicBlock();
    DebugLoc DL = MI.getDebugLoc();
    MachineFunction::iterator I = ++BB->getIterator();

    MachineBasicBlock *HeadMBB = BB;
    MachineFunction *F = BB->getParent();
    MachineBasicBlock *TailMBB = F->CreateMachineBasicBlock(LLVM_BB);
    MachineBasicBlock *IfFalseMBB = F->CreateMachineBasicBlock(LLVM_BB);

    F->insert(I, IfFalseMBB);
    F->insert(I, TailMBB);

    // Transfer debug instructions associated with the selects to TailMBB.
    for (MachineInstr *DebugInstr : SelectDebugValues) {
        TailMBB->push_back(DebugInstr->removeFromParent());
    }

    // Move all instructions after the sequence to TailMBB.
    TailMBB->splice(TailMBB->end(), HeadMBB,
                    std::next(LastSelectPseudo->getIterator()), HeadMBB->end());
    // Update machine-CFG edges by transferring all successors of the current
    // block to the new block which will contain the Phi nodes for the selects.
    TailMBB->transferSuccessorsAndUpdatePHIs(HeadMBB);
    // Set the successors for HeadMBB.
    HeadMBB->addSuccessor(IfFalseMBB);
    HeadMBB->addSuccessor(TailMBB);

    // Insert appropriate branch.
    unsigned Opcode = getBranchOpcodeForIntCondCode(CC);

    BuildMI(HeadMBB, DL, TII.get(TL45::SUB_TERM), TL45::r0)
            .addReg(LHS)
            .addReg(RHS);

    BuildMI(HeadMBB, DL, TII.get(Opcode))
            .addMBB(TailMBB);

    // IfFalseMBB just falls through to TailMBB.
    IfFalseMBB->addSuccessor(TailMBB);

    // Create PHIs for all of the select pseudo-instructions.
    auto SelectMBBI = MI.getIterator();
    auto SelectEnd = std::next(LastSelectPseudo->getIterator());
    auto InsertionPoint = TailMBB->begin();
    while (SelectMBBI != SelectEnd) {
        auto Next = std::next(SelectMBBI);
        if (isSelectPseudo(*SelectMBBI)) {
            // %Result = phi [ %TrueValue, HeadMBB ], [ %FalseValue, IfFalseMBB ]
            BuildMI(*TailMBB, InsertionPoint, SelectMBBI->getDebugLoc(),
                    TII.get(TL45::PHI), SelectMBBI->getOperand(0).getReg())
                    .addReg(SelectMBBI->getOperand(4).getReg())
                    .addMBB(HeadMBB)
                    .addReg(SelectMBBI->getOperand(5).getReg())
                    .addMBB(IfFalseMBB);
            SelectMBBI->eraseFromParent();
        }
        SelectMBBI = Next;
    }

    F->getProperties().reset(MachineFunctionProperties::Property::NoPHIs);
    return TailMBB;
}

MachineBasicBlock *
TL45TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                MachineBasicBlock *BB) const {
    switch (MI.getOpcode()) {
        default:
            llvm_unreachable("Unexpected instr type to insert");
//  case RISCV::ReadCycleWide:
//    assert(!Subtarget.is64Bit() &&
//           "ReadCycleWrite is only to be used on riscv32");
//    return emitReadCycleWidePseudo(MI, BB);
//  case RISCV::Select_GPR_Using_CC_GPR:
//  case RISCV::Select_FPR32_Using_CC_GPR:
//  case RISCV::Select_FPR64_Using_CC_GPR:
//    return emitSelectPseudo(MI, BB);
//  case RISCV::BuildPairF64Pseudo:
//    return emitBuildPairF64Pseudo(MI, BB);
//  case RISCV::SplitF64Pseudo:
//    return emitSplitF64Pseudo(MI, BB);
        case TL45::Select_GRRegs_Using_CC_GRRegs:
            return emitSelectPseudo(MI, BB);

    }
}

bool TL45TargetLowering::shouldReduceLoadWidth(SDNode *Load, ISD::LoadExtType ExtTy, EVT NewVT) const {
    // we don't support i16 loads right now, so don't merge into one.
//  if (NewVT == MVT::i16) {
//    return false;
//  }

    return TargetLoweringBase::shouldReduceLoadWidth(Load, ExtTy, NewVT);
}

bool TL45TargetLowering::canMergeStoresTo(unsigned AS, EVT MemVT, const SelectionDAG &DAG) const {
    return true; // MemVT == MVT::i8 || MemVT == MVT::i32;
}

