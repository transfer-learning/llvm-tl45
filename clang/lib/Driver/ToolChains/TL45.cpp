//
// Created by codetector on 2/7/20.
//

#include "TL45.h"

clang::driver::toolchains::TL45ToolChain::TL45ToolChain(
        const clang::driver::Driver &D, const llvm::Triple &Triple,
        const llvm::opt::ArgList &Args) : Generic_ELF(D, Triple, Args) {

}

void clang::driver::toolchains::TL45ToolChain::AddClangSystemIncludeArgs(
        const llvm::opt::ArgList &DriverArgs,
        llvm::opt::ArgStringList &CC1Args) const {
  ToolChain::AddClangSystemIncludeArgs(DriverArgs, CC1Args);
}

void clang::driver::toolchains::TL45ToolChain::addClangTargetOptions(
        const llvm::opt::ArgList &DriverArgs, llvm::opt::ArgStringList &CC1Args,
        clang::driver::Action::OffloadKind OffloadKind) const {
  Generic_ELF::addClangTargetOptions(DriverArgs, CC1Args, OffloadKind);
}

void clang::driver::tools::tl45::getTL45TargetFeatures(
        const clang::driver::Driver &D, const llvm::opt::ArgList &Args,
        std::vector<llvm::StringRef> &Features) {
}
