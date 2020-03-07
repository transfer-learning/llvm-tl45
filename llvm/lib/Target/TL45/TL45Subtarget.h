#ifndef TL45SUBTARGET_H
#define TL45SUBTARGET_H

#include "TL45.h"

#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "TL45FrameLowering.h"
#include "TL45ISelLowering.h"
#include "TL45InstrInfo.h"

#define GET_SUBTARGETINFO_HEADER

#include "TL45GenSubtargetInfo.inc"

namespace llvm {

class TL45Subtarget : public TL45GenSubtargetInfo {
private:
  virtual void anchor();

  TL45RegisterInfo RI;
  TL45InstrInfo InstrInfo;
  TL45TargetLowering TLInfo;
  SelectionDAGTargetInfo TSInfo;
//	InstrItineraryData InstrItins;
  TL45FrameLowering FrameLowering;

public:
  TL45Subtarget(const Triple &TT, const StringRef CPU, const StringRef FS,
                const TL45TargetMachine &TM);
  const TL45InstrInfo *getInstrInfo() const override { return &InstrInfo; }

  const TL45RegisterInfo *getRegisterInfo() const override {
    return &RI;
  }

  const TL45TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }

  const TL45FrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }

  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);

  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override;

};
}
//
#endif