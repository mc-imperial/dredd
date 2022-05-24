//
// Created by afd on 24/05/2022.
//

#ifndef DREDD_MUTATE_AST_VISITOR_H
#define DREDD_MUTATE_AST_VISITOR_H

namespace dredd {

class AddAtomicVisitor : public RecursiveASTVisitor<AddAtomicVisitor> {
public:
  using DeclWithIndirection = std::pair<const DeclaratorDecl*, int>;

  explicit AddAtomicVisitor(CompilerInstance *CI) : AstContext(&(CI->getASTContext())) { }

  const std::vector<const DeclaratorDecl*>& getDecls() const {
    return Decls;
  }

  const std::map<std::string, std::vector<const FunctionDecl*>>& getFunctionPrototypes() const {
    return FunctionPrototypes;
  }

  const std::map<const ParmVarDecl*, std::vector<const ParmVarDecl*>>& getParameterToPrototypeParameters() const {
    return ParameterToPrototypeParameters;
  }

  const std::unordered_map<const DeclaratorDecl*,
                           std::unordered_map<int, std::set<DeclWithIndirection>>>& getEquivalentTypes() const {
    return EquivalentTypes;
  }

  bool VisitDeclaratorDecl(DeclaratorDecl *DD) {
    if (ObservedDecls.count(DD) != 0) {
      assert(EquivalentTypes.count(DD) != 0);
      assert(dyn_cast<FunctionDecl>(DD) || dyn_cast<ParmVarDecl>(DD));
      return true;
    }
    assert(EquivalentTypes.count(DD) == 0);
    const DeclaratorDecl* CanonicalDD = DD;
    if (auto* FD = dyn_cast<FunctionDecl>(DD)) {
      const FunctionDecl* Definition = nullptr;
      if (FD->hasBody(Definition)) {
        CanonicalDD = Definition;
      }
      if (FD != Definition) {
        if (FunctionPrototypes.count(FD->getNameAsString()) == 0) {
          FunctionPrototypes.insert({FD->getNameAsString(), std::vector<const FunctionDecl*>()});
        }
        FunctionPrototypes.at(FD->getNameAsString()).push_back(FD);
        if (Definition != nullptr) {
          for (uint32_t I = 0; I < FD->getNumParams(); I++) {
            if (ParameterToPrototypeParameters.count(
                    Definition->getParamDecl(I)) == 0) {
              ParameterToPrototypeParameters.insert(
                  {Definition->getParamDecl(I),
                   std::vector<const ParmVarDecl *>()});
            }
            ParameterToPrototypeParameters.at(Definition->getParamDecl(I))
                .push_back(FD->getParamDecl(I));
            PrototypeParameterToCanonicalParameter.insert(
                {FD->getParamDecl(I), Definition->getParamDecl(I)});
          }
        }
      }
    }
    if (auto* PVD = dyn_cast<ParmVarDecl>(DD)) {
      if (PrototypeParameterToCanonicalParameter.count(PVD) != 0) {
        CanonicalDD = PrototypeParameterToCanonicalParameter.at(PVD);
      }
    }
    if (ObservedDecls.count(CanonicalDD) == 0) {
      ObservedDecls.insert(CanonicalDD);
      EquivalentTypes.insert({CanonicalDD, {}});
      Decls.push_back(CanonicalDD);
    }
    return true;
  }

  bool VisitExpr(Expr *E) {
    assert(EquivalentTypesInternal.count(E) == 0);
    EquivalentTypesInternal.insert({E, {}});
    return true;
  }

  bool TraverseFunctionDecl(FunctionDecl *FD) {
    assert(EnclosingFunction == nullptr);
    EnclosingFunction = FD;
    bool Result = RecursiveASTVisitor::TraverseFunctionDecl(FD);
    assert(EnclosingFunction == FD);
    EnclosingFunction = nullptr;
    return Result;
  }

  bool TraverseVarDecl(VarDecl *VD) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseVarDecl(VD);
    if (VD->hasInit()) {
      handleAssignment({VD, 0}, *VD->getInit());
    }
    return true;
  }

  bool TraverseMemberExpr(MemberExpr *ME) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseMemberExpr(ME);
    auto* FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
    assert(FD != nullptr);
    EquivalentTypesInternal.at(ME).insert({FD, 0});
    return true;
  }

  bool TraverseDeclRefExpr(DeclRefExpr *DRE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseDeclRefExpr(DRE);
    if (auto* DD = dyn_cast<DeclaratorDecl>(DRE->getDecl())) {
      EquivalentTypesInternal.at(DRE).insert({DD, 0});
    }
    return true;
  }

  bool TraverseCallExpr(CallExpr* CE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseCallExpr(CE);
    if (auto* DirectCallee = CE->getDirectCallee()) {
      FunctionDecl* FD = DirectCallee->isDefined() ? DirectCallee->getDefinition() : DirectCallee;
      EquivalentTypesInternal.at(CE).insert({FD, 0});
      for (size_t I = 0; I < FD->getNumParams(); I++) {
        handleAssignment({FD->getParamDecl(I), 0}, *CE->getArg(I));
      }
    }
    return true;
  }

  bool TraverseReturnStmt(ReturnStmt* RS) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseReturnStmt(RS);
    assert(EnclosingFunction != nullptr);
    handleAssignment({EnclosingFunction, 0}, *RS->getRetValue());
    return true;
  }

  bool TraverseImplicitCastExpr(ImplicitCastExpr* ICE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseImplicitCastExpr(ICE);
    handlePassUp(ICE, ICE->getSubExpr());
    return true;
  }

  bool TraverseParenExpr(ParenExpr* PE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseParenExpr(PE);
    handlePassUp(PE, PE->getSubExpr());
    return true;
  }

  bool TraverseConditionalOperator(ConditionalOperator* CO) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseConditionalOperator(CO);
    handlePassUp(CO, CO->getTrueExpr());
    handlePassUp(CO, CO->getFalseExpr());
    return true;
  }

  bool TraverseArraySubscriptExpr(ArraySubscriptExpr* ASE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseArraySubscriptExpr(ASE);
    for (auto& DDWithIndirection : EquivalentTypesInternal.at(ASE->getBase())) {
      EquivalentTypesInternal.at(ASE).insert({DDWithIndirection.first, DDWithIndirection.second + 1});
    }
    return true;
  }

  bool TraverseUnaryOperator(UnaryOperator* UO) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseUnaryOperator(UO);
    auto* SubExpr = UO->getSubExpr();
    switch(UO->getOpcode()) {
    case clang::UO_AddrOf: {
      for (auto& DDWithIndirection : EquivalentTypesInternal.at(SubExpr)) {
        EquivalentTypesInternal.at(UO).insert({DDWithIndirection.first, DDWithIndirection.second - 1});
      }
      break;
    }
    case clang::UO_Deref: {
      for (auto& DDWithIndirection : EquivalentTypesInternal.at(SubExpr)) {
        EquivalentTypesInternal.at(UO).insert({DDWithIndirection.first, DDWithIndirection.second + 1});
      }
      break;
    }
    default:
      break;
    }
    return true;
  }

  bool TraverseBinaryOperator(BinaryOperator *BO) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseBinaryOperator(BO);
    switch(BO->getOpcode()) {
    case clang::BO_EQ:
    case clang::BO_GE:
    case clang::BO_GT:
    case clang::BO_LE:
    case clang::BO_LT: {
      makeEquivalent(*BO->getLHS(), *BO->getRHS());
      break;
    }
    case clang::BO_Assign: {
      makeEquivalent(*BO->getLHS(), *BO->getRHS());
      handlePassUp(BO, BO->getLHS());
      break;
    }
    case clang::BO_Comma: {
      handlePassUp(BO, BO->getRHS());
      break;
    }
    default:
      break;
    }
    return true;
  }

private:

  void handlePassUp(Expr* E, Expr *SubExpr) {
    EquivalentTypesInternal.at(E).insert(EquivalentTypesInternal.at(SubExpr).begin(), EquivalentTypesInternal.at(SubExpr).end());
  }

  void handleAssignment(const DeclWithIndirection& DDWithIndirection, const Expr& E) {
    if (auto* ILE = dyn_cast<InitListExpr>(&E)) {
      QualType QT = DDWithIndirection.first->getType();
      if (auto* RT = QT->getAs<RecordType>()) {
        auto FieldIterator = RT->getDecl()->field_begin();
        for (uint32_t I = 0; I < ILE->getNumInits(); I++) {
          handleAssignment({*FieldIterator, 0}, *ILE->getInit(I));
          ++FieldIterator;
        }
      } else if (QT->isArrayType()) {
        for (uint32_t I = 0; I < ILE->getNumInits(); I++) {
          handleAssignment({DDWithIndirection.first, DDWithIndirection.second + 1}, *ILE->getInit(I));
        }
      } else {
        errs() << "Unexpected initializer list\n";
        exit(1);
      }
    } else {
      for (auto& OtherDDWithIndirection : EquivalentTypesInternal.at(&E)) {
        addEquivalenceOneWay(DDWithIndirection, OtherDDWithIndirection);
        addEquivalenceOneWay(OtherDDWithIndirection, DDWithIndirection);
      }
    }
  }

  void addEquivalenceOneWay(const DeclWithIndirection& DWI1, const DeclWithIndirection& DWI2) {
    if (EquivalentTypes.at(DWI1.first).count(DWI1.second) == 0) {
      EquivalentTypes.at(DWI1.first).insert({DWI1.second, {}});
    }
    EquivalentTypes.at(DWI1.first).at(DWI1.second).insert(DWI2);
  }

  void makeEquivalent(const Expr& E1, const Expr& E2) {
    for (auto& DDWI1 : EquivalentTypesInternal.at(&E1)) {
      for (auto& DDWI2 : EquivalentTypesInternal.at(&E2)) {
        addEquivalenceOneWay(DDWI1, DDWI2);
        addEquivalenceOneWay(DDWI2, DDWI1);
      }
    }
  }

  ASTContext* AstContext;

  // The declarations we have processed, in the order in which we found them.
  std::vector<const DeclaratorDecl*> Decls;

  std::unordered_map<const DeclaratorDecl*,
                     std::unordered_map<int, std::set<DeclWithIndirection>>> EquivalentTypes;

  // Prototypes (without bodies) of all functions.
  std::map<std::string, std::vector<const FunctionDecl*>> FunctionPrototypes;

  // Mapping from parameters of a function definition to all the corresponding parameters of its prototypes.
  std::map<const ParmVarDecl*, std::vector<const ParmVarDecl*>> ParameterToPrototypeParameters;
  std::map<const ParmVarDecl*, const ParmVarDecl*> PrototypeParameterToCanonicalParameter;

  // Intermediate variables for bottom-up processing:

  // The declarations we have seen in a form that can be efficiently queried.
  // Used so that we do not add to |Decls| a declaration we already processed.
  std::unordered_set<const DeclaratorDecl*> ObservedDecls;

  std::unordered_map<const Expr*, std::set<DeclWithIndirection>> EquivalentTypesInternal;

  // Tracks the function currently being traversed, if any, so that when we
  // process a 'return' statement we know which function it relates to.
  FunctionDecl* EnclosingFunction = nullptr;

  // End of intermediate variables.

};

#endif // DREDD_MUTATE_AST_VISITOR_H
