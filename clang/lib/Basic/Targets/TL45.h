
//
// Created by codetector on 9/7/19.
//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_TL45_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_TL45_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY TL45TargetInfo : public TargetInfo {
  static const TargetInfo::GCCRegAlias GCCRegAliases[];
  static const char *const GCCRegNames[];
//  static const Builtin::Info BuiltinInfo[];
public:
  TL45TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts) : TargetInfo(Triple) {
    resetDataLayout(
            "E-m:e-p:32:32-i8:8-i16:16-i32:32-i64:32-a:0:32-n8:16:32-S32"
    );
    PointerWidth = PointerAlign = 32;
    BoolWidth = BoolAlign = 8;
    IntWidth = IntAlign = 32;
    LongWidth = LongAlign = 32;
    LongLongWidth = LongLongAlign = 64;
    BigEndian = true;
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<GCCRegAlias> getGCCRegAliases() const override;

  ArrayRef<Builtin::Info> getTargetBuiltins() const override;

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override {
    return false;
  }

  const char *getClobbers() const override { return ""; }

};
}
}

#endif //LLVM_CLANG_LIB_BASIC_TARGETS_TL45_H
