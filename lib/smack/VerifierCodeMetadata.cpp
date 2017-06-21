//
// This file is distributed under the MIT License. See LICENSE for details.
//

#define DEBUG_TYPE "verifier-code-metadata"

#include "smack/SmackOptions.h"
#include "smack/Naming.h"
#include "smack/VerifierCodeMetadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"

#include <vector>
#include <stack>
#include <map>
#include <set>

namespace smack {

using namespace llvm;

namespace {
  std::string getString(const llvm::Value* v) {
    if (const llvm::ConstantExpr* constantExpr = llvm::dyn_cast<const llvm::ConstantExpr>(v))
      if (constantExpr->getOpcode() == llvm::Instruction::GetElementPtr)
        if (const llvm::GlobalValue* cc = llvm::dyn_cast<const llvm::GlobalValue>(constantExpr->getOperand(0)))
          if (const llvm::ConstantDataSequential* cds = llvm::dyn_cast<const llvm::ConstantDataSequential>(cc->getOperand(0)))
            return cds ->getAsCString();
    return "";
  }

  void mark(Instruction &I, bool V = true) {
    auto& C = I.getContext();
    I.setMetadata("verifier.code",
      MDNode::get(C, ConstantAsMetadata::get(
        V ? ConstantInt::getTrue(C) : ConstantInt::getFalse(C)
      )));
  }

  bool isMarked(Instruction& I) {
    auto *N = I.getMetadata("verifier.code");
    assert(N && "expected metadata");
    assert(N->getNumOperands() == 1);
    auto *M = dyn_cast<ConstantAsMetadata>(N->getOperand(0).get());
    assert(M && "expected constant-valued metadata");
    auto *C = dyn_cast<ConstantInt>(M->getValue());
    assert(C && "expected constant-int-valued metadata");
    return C->isOne();
  }

  bool isVerifierFunctionCall(CallInst &I) {
    if (auto F = I.getCalledFunction()) {
      auto N = F->getName();

      if (N.find("__VERIFIER_") == 0)
        return true;

      if (N.find("__SMACK") == 0)
        return true;

      if (N.find("__CONTRACT") == 0)
        return true;
    }
    return false;
  }

  bool onlyVerifierUsers(Instruction &I) {
    std::queue<User*> users;
    std::set<User*> known;

    for (auto U : I.users()) {
      users.push(U);
      known.insert(U);
    }

    while (!users.empty()) {
      if (auto K = dyn_cast<Instruction>(users.front())) {
        if (!isMarked(*K))
          return false;

      } else {
        for (auto UU : users.front()->users()) {
          if (known.count(UU) == 0) {
            users.push(UU);
            known.insert(UU);
          }
        }
      }
      users.pop();
    }
    return true;
  }
}

void VerifierCodeMetadata::getAnalysisUsage(AnalysisUsage &AU) const {
}

bool VerifierCodeMetadata::runOnModule(Module &M) {

  // first mark verifier function calls
  visit(M);

  // then mark values which flow only into marked instructions
  while (!workList.empty()) {
    auto &I = *workList.front();
    for (auto V : I.operand_values()) {
      if (auto J = dyn_cast<Instruction>(V)) {
        if (!isMarked(*J) && !dyn_cast<CallInst>(J)) {
          if (onlyVerifierUsers(*J)) {
            mark(*J);
            workList.push(J);
          }
        }
      }
    }
    workList.pop();
  }

  return true;
}

void VerifierCodeMetadata::visitCallInst(CallInst &I) {
  auto marked = false;

  if (isVerifierFunctionCall(I)) {
    marked = true;
    workList.push(&I);
  }

  mark(I, marked);
}

void VerifierCodeMetadata::visitInstruction(Instruction &I) {
  mark(I, false);
}

// Pass ID variable
char VerifierCodeMetadata::ID = 0;

// Register the pass
static RegisterPass<VerifierCodeMetadata>
X("verifier-code-metadata", "Verifier Code Metadata");

}
