//===-- DistanceCalculator.h ------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_DISTANCE_CALCULATOR_H
#define KLEE_DISTANCE_CALCULATOR_H

#include "ExecutionState.h"

namespace llvm {
class BasicBlock;
} // namespace llvm

namespace klee {
class CodeGraphDistance;
class Target;

enum WeightResult : std::uint8_t {
  Done = 0U,
  Continue = 1U,
  Miss = 2U,
};

using weight_type = unsigned;

struct DistanceResult {
  WeightResult result;
  weight_type weight;
  bool isInsideFunction;

  explicit DistanceResult(WeightResult result_ = WeightResult::Miss,
                          weight_type weight_ = 0,
                          bool isInsideFunction_ = true)
      : result(result_), weight(weight_), isInsideFunction(isInsideFunction_){};

  bool operator<(const DistanceResult &b) const;

  std::string toString() const;
};

class DistanceCalculator {
public:
  explicit DistanceCalculator(CodeGraphDistance &codeGraphDistance_)
      : codeGraphDistance(codeGraphDistance_) {}

  DistanceResult getDistance(const ExecutionState &es, KBlock *target);

  DistanceResult getDistance(const KInstruction *prevPC, const KInstruction *pc,
                             const ExecutionStack::call_stack_ty &frames,
                             KBlock *target);

private:
  enum TargetKind : std::uint8_t {
    LocalTarget = 0U,
    PreTarget = 1U,
    PostTarget = 2U,
    NoneTarget = 3U,
  };

  struct SpeculativeState {
  private:
    unsigned hashValue;

    unsigned computeHash();

  public:
    KBlock *kb;
    TargetKind kind;
    SpeculativeState(KBlock *kb_, TargetKind kind_) : kb(kb_), kind(kind_) {
      computeHash();
    }
    ~SpeculativeState() = default;
    unsigned hash() const { return hashValue; }
  };

  struct SpeculativeStateHash {
    bool operator()(const SpeculativeState &a) const { return a.hash(); }
  };

  struct SpeculativeStateCompare {
    bool operator()(const SpeculativeState &a,
                    const SpeculativeState &b) const {
      return a.kb == b.kb && a.kind == b.kind;
    }
  };

  using SpeculativeStateToDistanceResultMap =
      std::unordered_map<SpeculativeState, DistanceResult, SpeculativeStateHash,
                         SpeculativeStateCompare>;
  using TargetToSpeculativeStateToDistanceResultMap =
      std::unordered_map<KBlock *, SpeculativeStateToDistanceResultMap>;

  using StatesSet = std::unordered_set<ExecutionState *>;

  CodeGraphDistance &codeGraphDistance;
  TargetToSpeculativeStateToDistanceResultMap distanceResultCache;
  StatesSet localStates;

  DistanceResult getDistance(KBlock *kb, TargetKind kind, KBlock *target);

  DistanceResult computeDistance(KBlock *kb, TargetKind kind,
                                 KBlock *target) const;

  bool distanceInCallGraph(KFunction *kf, KBlock *kb, unsigned int &distance,
                           const std::unordered_map<KFunction *, unsigned int>
                               &distanceToTargetFunction,
                           KBlock *target) const;
  bool distanceInCallGraph(KFunction *kf, KBlock *kb, unsigned int &distance,
                           const std::unordered_map<KFunction *, unsigned int>
                               &distanceToTargetFunction,
                           KBlock *target, bool strictlyAfterKB) const;

  WeightResult tryGetLocalWeight(KBlock *kb, weight_type &weight,
                                 const std::vector<KBlock *> &localTargets,
                                 KBlock *target) const;
  WeightResult
  tryGetPreTargetWeight(KBlock *kb, weight_type &weight,
                        const std::unordered_map<KFunction *, unsigned int>
                            &distanceToTargetFunction,
                        KBlock *target) const;
  WeightResult tryGetTargetWeight(KBlock *kb, weight_type &weight,
                                  KBlock *target) const;
  WeightResult tryGetPostTargetWeight(KBlock *kb, weight_type &weight,
                                      KBlock *target) const;
};
} // namespace klee

#endif /* KLEE_DISTANCE_CALCULATOR_H */
