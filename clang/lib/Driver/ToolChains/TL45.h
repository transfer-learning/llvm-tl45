//
// Created by codetector on 2/7/20.
//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_TL45_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_TL45_H

#include "Gnu.h"
#include "InputInfo.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/Option.h"

#include <string>
#include <vector>

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY TL45ToolChain : public Generic_ELF {
public:
  TL45ToolChain(const Driver &D, const llvm::Triple &Triple,
                const llvm::opt::ArgList &Args);

  void AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                                 llvm::opt::ArgStringList &CC1Args) const override;

  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind OffloadKind) const override;

  bool isPICDefault() const override {
    return false;
  }

  bool isPIEDefault() const override {
    return false;
  }

  bool isPICDefaultForced() const override {
    return true;
  }
};

} // end namespace toolchains

namespace tools {
namespace tl45 {

void getTL45TargetFeatures(const Driver &D, const llvm::opt::ArgList &Args,
        std::vector<llvm::StringRef> &Features);

} // end namespace tl45
} // end namespace tools
} // end namespace driver
} // end namespace clang


#endif //LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_TL45_H
