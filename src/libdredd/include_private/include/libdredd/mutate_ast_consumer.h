//
// Created by afd on 24/05/2022.
//

#ifndef DREDD_MUTATE_AST_CONSUMER_H
#define DREDD_MUTATE_AST_CONSUMER_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace dredd {

class MutateAstConsumer : public clang::ASTConsumer {
public:
  MutateAstConsumer(const clang::CompilerInstance &ci,
                                size_t num_mutations,
                                const std::string &output_file, std::mt19937& generator)
      : ci_(ci), num_mutations_(num_mutations), output_file_(output_file), generator_(generator)
        // ,        Visitor(std::make_unique<MutateVisitor>(ci))
        {}

//  void
//  showDeclWithIndirection(const AddAtomicVisitor::DeclWithIndirection &DDWI) {
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

  void HandleTranslationUnit(clang::ASTContext &context) override {
    if (context.getDiagnostics().hasErrorOccurred()) {
      // There has been an error, so we don't do any processing.
      return;
    }
//    Visitor->TraverseDecl(Context.getTranslationUnitDecl());
//
//    for (auto *DD : Visitor->getDecls()) {
//      errs() << DD->getNameAsString() << "\n";
//      for (auto &Entry : Visitor->getEquivalentTypes().at(DD)) {
//        for (auto &InnerEntry : Entry.second) {
//          errs() << "   ";
//          showDeclWithIndirection({DD, Entry.first});
//          errs() << " ~ ";
//          showDeclWithIndirection(InnerEntry);
//          errs() << "\n";
//        }
//      }
//    }
//
//    const DeclaratorDecl *InitialUpgrade = nullptr;
//    if (NameToUpgrade.empty()) {
//      while (true) {
//        int Index = std::uniform_int_distribution<size_t>(
//            0, Visitor->getDecls().size() - 1)(*MT);
//        auto *DD = Visitor->getDecls()[Index];
//        if (const auto *FD = dyn_cast<FieldDecl>(DD)) {
//          if (FD->isBitField()) {
//            // We cannot make bitfields atomic.
//            continue;
//          }
//        }
//        if (DD->getType()->isArrayType()) {
//          // If we have a declaration like "int A[5] = ..." then 'A' will have
//          // array type and we cannot make the array declaration atomic.
//          continue;
//        }
//        if (dyn_cast<DecayedType>(DD->getType())) {
//          // Arrays and functions decay to pointers, and we cannot make either
//          // type atomic.
//          continue;
//        }
//        if (CI->getSourceManager().getFileID(DD->getBeginLoc()) ==
//            CI->getSourceManager().getMainFileID()) {
//          InitialUpgrade = DD;
//          break;
//        }
//      }
//    } else {
//      for (auto *DD : Visitor->getDecls()) {
//        if (DD->getNameAsString() == NameToUpgrade) {
//          InitialUpgrade = DD;
//          break;
//        }
//      }
//      if (InitialUpgrade == nullptr) {
//        errs() << "Did not find a declarator declaration named "
//               << NameToUpgrade << "\n";
//        exit(1);
//      }
//    }
//
//    std::unordered_map<const DeclaratorDecl *, size_t> Upgrades;
//    errs() << "Initially upgrading " << InitialUpgrade->getDeclName() << "\n";
//    Upgrades.insert({InitialUpgrade, 0});
//    std::deque<std::pair<const DeclaratorDecl *, size_t>> UpgradesToPropagate;
//    UpgradesToPropagate.push_back({InitialUpgrade, 0});
//    while (!UpgradesToPropagate.empty()) {
//      std::pair<const DeclaratorDecl *, size_t> Current =
//          UpgradesToPropagate.front();
//      UpgradesToPropagate.pop_front();
//      errs() << "Propagating upgrade " << Current.first->getDeclName() << " "
//             << Current.second << "\n";
//      for (auto &Entry : Visitor->getEquivalentTypes().at(Current.first)) {
//        int ReconciledIndirectionLevel =
//            static_cast<int>(Current.second) - Entry.first;
//        if (ReconciledIndirectionLevel <= 0) {
//          continue;
//        }
//        for (auto &DDWI : Entry.second) {
//          int Temp =
//              static_cast<int>(Current.second) + (DDWI.second - Entry.first);
//          assert(Temp >= 0);
//          size_t NewLevel = static_cast<size_t>(Temp);
//          if (Upgrades.count(DDWI.first) != 0) {
//            assert(Upgrades.at(DDWI.first) == NewLevel);
//          } else {
//            Upgrades.insert({DDWI.first, NewLevel});
//            UpgradesToPropagate.push_back({DDWI.first, NewLevel});
//          }
//        }
//      }
//    }
//    errs() << "Upgrades:\n";
//    for (auto &Entry : Upgrades) {
//      errs() << Entry.first->getDeclName() << " " << Entry.second << "\n";
//    }
//
//    TheRewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts());
//    for (auto &Entry : Upgrades) {
//      rewriteType(Entry.first->getTypeSourceInfo()->getTypeLoc(), Entry.second);
//      if (auto *FD = dyn_cast<FunctionDecl>(Entry.first)) {
//        if (Visitor->getFunctionPrototypes().count(FD->getNameAsString())) {
//          for (auto &Prototype :
//               Visitor->getFunctionPrototypes().at(FD->getNameAsString())) {
//            if (FD != Prototype) {
//              // Upgrade the prototype to stay in sync with the definition.
//              rewriteType(Prototype->getTypeSourceInfo()->getTypeLoc(),
//                          Entry.second);
//            }
//          }
//        }
//      }
//      if (auto *PVD = dyn_cast<ParmVarDecl>(Entry.first)) {
//        if (Visitor->getParameterToPrototypeParameters().count(PVD)) {
//          for (auto *PrototypeParameter :
//               Visitor->getParameterToPrototypeParameters().at(PVD)) {
//            assert(PVD != PrototypeParameter);
//            rewriteType(PrototypeParameter->getTypeSourceInfo()->getTypeLoc(),
//                        Entry.second);
//          }
//        }
//      }
//    }
//
//    const RewriteBuffer *RewriteBuf =
//        TheRewriter.getRewriteBufferFor(CI->getSourceManager().getMainFileID());
//
//    std::ofstream OutputFileStream(OutputFile, std::ofstream::out);
//    OutputFileStream << std::string(RewriteBuf->begin(), RewriteBuf->end());
//    OutputFileStream.close();
  }

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

  const clang::CompilerInstance &ci_;
  const size_t num_mutations_;
  const std::string &output_file_;
  std::mt19937& generator_;
  //std::unique_ptr<MutateVisitor> visitor_;
  clang::Rewriter rewriter_;
};

} // dredd

#endif // DREDD_MUTATE_AST_CONSUMER_H
