// Copyright 2022 The Dredd Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DREDD_MUTATE_AST_CONSUMER_H
#define DREDD_MUTATE_AST_CONSUMER_H

#include <cstddef>
#include <random>
#include <string>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace dredd {

class MutateAstConsumer : public clang::ASTConsumer {
 public:
  MutateAstConsumer(const clang::CompilerInstance& ci, size_t num_mutations,
                    const std::string& output_file, std::mt19937& generator)
      : ci_(ci),
        num_mutations_(num_mutations),
        output_file_(output_file),
        generator_(generator)
  // ,        Visitor(std::make_unique<MutateVisitor>(ci))
  {}

  //  void
  //  showDeclWithIndirection(const AddAtomicVisitor::DeclWithIndirection &DDWI)
  //  {
  //    if (DDWI.second == -1) {
  //      errs() << "&";
  //    } else {
  //      assert(DDWI.second >= 0);
  //      for (int i = 0; i < DDWI.second; i++) {
  //        errs() << "*";
  //      }
  //    }
  //    errs() << DDWI.first->getDeclName();
  //  }

  void HandleTranslationUnit(clang::ASTContext& context) override;

 private:
  //  void rewriteType(const TypeLoc &TL, size_t IndirectionLevel) {
  //    if (TL.getTypeLocClass() == TypeLoc::Qualified) {
  //      rewriteType(TL.castAs<QualifiedTypeLoc>().getUnqualifiedLoc(),
  //                  IndirectionLevel);
  //      return;
  //    }
  //    if (TL.getTypeLocClass() == TypeLoc::FunctionProto) {
  //      rewriteType(TL.castAs<FunctionProtoTypeLoc>().getReturnLoc(),
  //                  IndirectionLevel);
  //      return;
  //    }
  //    if (TL.getTypeLocClass() == TypeLoc::FunctionNoProto) {
  //      rewriteType(TL.castAs<FunctionNoProtoTypeLoc>().getReturnLoc(),
  //                  IndirectionLevel);
  //      return;
  //    }
  //    if (IndirectionLevel == 0) {
  //      assert(TL.getTypeLocClass() != TypeLoc::IncompleteArray);
  //      assert(TL.getTypeLocClass() != TypeLoc::ConstantArray);
  //      TheRewriter.InsertTextAfterToken(TL.getEndLoc(), " _Atomic ");
  //      return;
  //    }
  //    if (TL.getTypeLocClass() == TypeLoc::Pointer) {
  //      rewriteType(TL.castAs<PointerTypeLoc>().getPointeeLoc(),
  //                  IndirectionLevel - 1);
  //      return;
  //    }
  //    if (TL.getTypeLocClass() == TypeLoc::ConstantArray) {
  //      rewriteType(TL.castAs<ConstantArrayTypeLoc>().getElementLoc(),
  //                  IndirectionLevel - 1);
  //      return;
  //    }
  //    errs() << "Unhandled type loc " << TL.getTypeLocClass() << "\n";
  //    exit(1);
  //  }

  const clang::CompilerInstance& ci_;
  const size_t num_mutations_;
  const std::string& output_file_;
  std::mt19937& generator_;
  // std::unique_ptr<MutateVisitor> visitor_;
  clang::Rewriter rewriter_;
};

}  // namespace dredd

#endif  // DREDD_MUTATE_AST_CONSUMER_H
