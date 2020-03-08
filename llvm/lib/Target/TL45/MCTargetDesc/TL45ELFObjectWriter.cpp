//
// Created by codetector on 9/3/19.
//
#include <memory>
#include "MCTargetDesc/TL45BaseInfo.h"
#include "MCTargetDesc/TL45FixupKinds.h"
#include "MCTargetDesc/TL45MCTargetDesc.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
    class TL45ELFObjectWriter : public MCELFObjectTargetWriter {
    public:
        TL45ELFObjectWriter(uint8_t OSABI);

        virtual ~TL45ELFObjectWriter();

      // Return true if the given relocation must be with a symbol rather than
      // section plus offset.
      bool needsRelocateWithSymbol(const MCSymbol &Sym,
                                   unsigned Type) const override {
        // TODO: this is very conservative, update once RISC-V psABI requirements
        //       are clarified.
        return true;
      }

    protected:
        unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                const MCFixup &Fixup, bool IsPCRel) const override;
    };
}

TL45ELFObjectWriter::TL45ELFObjectWriter(uint8_t OSABI) : MCELFObjectTargetWriter(false, OSABI, ELF::EM_TL45, false){}

TL45ELFObjectWriter::~TL45ELFObjectWriter() {}

unsigned TL45ELFObjectWriter::getRelocType(MCContext &Ctx,
                                            const MCValue &Target,
                                            const MCFixup &Fixup,
                                            bool IsPCRel) const {
  const MCExpr *Expr = Fixup.getValue();
  // Determine the type of the relocation
  unsigned Kind = Fixup.getKind();
  if (IsPCRel) {
      llvm_unreachable("invalid fixup kind! (PcRel)");
  }

  switch (Kind) {
  default:
    llvm_unreachable("invalid fixup kind!");
  case TL45::fixup_tl45_hi16_i:
    return ELF::R_TL45_HI16_I;
  case TL45::fixup_tl45_lo16_i:
    return ELF::R_TL45_LO16_I;

  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case TL45::fixup_tl45_32:
    return ELF::R_TL45_32;

//  case RISCV::fixup_riscv_relax:
//    return ELF::R_RISCV_RELAX;
//  case RISCV::fixup_riscv_align:
//    return ELF::R_RISCV_ALIGN;
  }
}

std::unique_ptr<MCObjectTargetWriter> llvm::createTL45ELFObjectWriter(uint8_t OSABI) {
    return make_unique<TL45ELFObjectWriter>(OSABI);
}
