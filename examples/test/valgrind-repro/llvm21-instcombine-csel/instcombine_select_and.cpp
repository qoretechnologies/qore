// Copyright (C) 2026 Qore Technologies, s.r.o.
//
// Standalone reproduction (no Qore code) of the report of LLVM 21.1 on aarch64 that the suppression
// llvm21-instcombine-select-binop-arm64-csel in examples/test/valgrind.supp covers (qore#7729):
//   Conditional jump or move depends on uninitialised value(s)
//   at ??? (libLLVM.so.21.1)  [InstCombinerImpl::SimplifySelectsFeedingBinaryOp]
//   by ??? (libLLVM.so.21.1)  [InstCombinerImpl::foldUsingDistributiveLaws]
//   by ??? (libLLVM.so.21.1)  [InstCombinerImpl::visitAnd]
//   by ??? (libLLVM.so.21.1)  [InstCombinerImpl::run]
//   by ??? (libLLVM.so.21.1)  [combineInstructionsOverFunction]
//   by llvm::InstCombinePass::run
// It makes the calls of Qore's optimizeModule() (lib/QoreJIT.cpp: a native TargetMachine, PassBuilder, the O2
// per-module default pipeline) on a module of one function whose "and" has a select as its first operand and a
// value that is not a select as its second (the function of f.ll, built with IRBuilder).  It is not built by the
// build of Qore; build and run it by hand:
//   c++ -std=c++17 instcombine_select_and.cpp $(llvm-config-21 --cxxflags --ldflags --libs) -o instcombine_select_and
//   valgrind --track-origins=yes ./instcombine_select_and
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <string>

int main() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::LLVMContext ctx;
    llvm::Module module("repro", ctx);
    llvm::Triple triple(llvm::sys::getProcessTriple());
    module.setTargetTriple(triple);

    llvm::Type* i1 = llvm::Type::getInt1Ty(ctx);
    llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
    llvm::Function* f = llvm::Function::Create(llvm::FunctionType::get(i64, {i1, i64}, false),
        llvm::Function::ExternalLinkage, "f", module);
    llvm::IRBuilder<> b(llvm::BasicBlock::Create(ctx, "entry", f));
    llvm::Value* s = b.CreateSelect(f->getArg(0), b.getInt64(64), b.getInt64(32));
    b.CreateRet(b.CreateAnd(s, f->getArg(1)));
    if (llvm::verifyModule(module, &llvm::errs())) {
        return 1;
    }

    std::string err;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        fprintf(stderr, "no target: %s\n", err.c_str());
        return 1;
    }
    std::unique_ptr<llvm::TargetMachine> tm(target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_));
    if (!tm) {
        return 1;
    }
    module.setDataLayout(tm->createDataLayout());

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    llvm::PassBuilder PB(tm.get(), llvm::PipelineTuningOptions(), std::nullopt, nullptr);
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    MPM.run(module, MAM);

    module.print(llvm::outs(), nullptr);
    return 0;
}
