//
// Created by codetector on 9/3/19.
//

#include "MCTargetDesc/TL45MCTargetDesc.h"
// #include "MCTargetDesc/TL45FixupKinds.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCAsmLayout.h"
#include "MCTargetDesc/TL45FixupKinds.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class TL45AsmBackend : public MCAsmBackend {
  Triple::OSType OSType;
public:
  TL45AsmBackend(const Target &T, Triple::OSType _OSType) : MCAsmBackend(support::endianness::big),
                                                              OSType(_OSType) {
  }

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved, const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter> createObjectTargetWriter() const override;

   unsigned int getNumFixupKinds() const override;

  bool mayNeedRelaxation(const MCInst &Inst, const MCSubtargetInfo &STI) const override;

  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value, const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override;

  void relaxInstruction(const MCInst &Inst, const MCSubtargetInfo &STI, MCInst &Res) const override;

  bool writeNopData(raw_ostream &OS, uint64_t Count) const override;

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;
};
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  switch (Fixup.getKind()) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    return Value;
  case TL45::fixup_tl45_lo16_i:
    return Value & 0xffff;
  case TL45::fixup_tl45_hi16_i:
    return (Value >> 16) & 0xffff;
  }
}

void TL45AsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, const MCValue &Target,
                                  MutableArrayRef<char> Data, uint64_t Value, bool IsResolved,
                                  const MCSubtargetInfo *STI) const {
  MCContext &Ctx = Asm.getContext();
  MCFixupKindInfo Info = getFixupKindInfo(Fixup.getKind());
  if (!Value)
    return; // Doesn't change encoding.
  // Apply any target-specific value adjustments.
  Value = adjustFixupValue(Fixup, Value, Ctx);

  // Shift the value into position.
  Value <<= Info.TargetOffset;

  unsigned Offset = Fixup.getOffset();
  unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;

  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  for (unsigned i = 0; i != NumBytes; ++i) {
    Data[Offset + i] |= uint8_t((Value >> (i * 8)) & 0xff);
  }
}

std::unique_ptr<MCObjectTargetWriter> TL45AsmBackend::createObjectTargetWriter() const {
  if (OSType == Triple::OSType::UnknownOS) {
    return createTL45ELFObjectWriter(0);
  }
  llvm_unreachable("Can not process the specified OS");
}

unsigned int TL45AsmBackend::getNumFixupKinds() const {
  return TL45::Fixups::NumTargetFixupKinds;
}

bool TL45AsmBackend::mayNeedRelaxation(const MCInst &Inst, const MCSubtargetInfo &STI) const {
  return false;
}

bool TL45AsmBackend::fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value, const MCRelaxableFragment *DF,
                                            const MCAsmLayout &Layout) const {
  return false;
}

void TL45AsmBackend::relaxInstruction(const MCInst &Inst, const MCSubtargetInfo &STI, MCInst &Res) const {
}

bool TL45AsmBackend::writeNopData(raw_ostream &OS, uint64_t Count) const {
  if (Count == 0) {
    return true;
  }
  return false;
}

const MCFixupKindInfo &TL45AsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[] = {
      // This table *must* be in the order that the fixup_* kinds are defined in
      // TL45FixupKinds.h.
      //
      // name                      offset bits  flags
      { "fixup_tl45_hi16_i",  0,     16,  0 },
      { "fixup_tl45_lo16_i",  0,     16,  0 },
      { "fixup_tl45_32",      0,     32,  0 },
  };
  static_assert((array_lengthof(Infos)) == TL45::NumTargetFixupKinds,
                "Not all fixup kinds added to Infos array");

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
             "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}


MCAsmBackend *llvm::createTL45AsmBackend(const llvm::Target &T, const llvm::MCSubtargetInfo &STI,
                                           const llvm::MCRegisterInfo &MRI, const llvm::MCTargetOptions &Options) {
  return new TL45AsmBackend(T, STI.getTargetTriple().getOS());
}
