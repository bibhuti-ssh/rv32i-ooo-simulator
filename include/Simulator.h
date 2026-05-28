#pragma once

#include "BranchPredictor.h"
#include "CacheHierarchy.h"
#include "Common.h"
#include "Decoder.h"
#include "ElfLoader.h"
#include "ExecutionUnits.h"
#include "InstructionFetch.h"
#include "IssueQueue.h"
#include "Memory.h"
#include "PhysicalRegisterFile.h"
#include "RenameUnit.h"
#include "ReorderBuffer.h"
#include "Stats.h"

namespace sim {

class Simulator {
public:
  Simulator(SimulatorConfig config, std::string programPath);
  const Stats &run();
  const Stats &stats() const { return stats_; }
  const BranchPredictor &branchPredictor() const { return predictor_; }
  const CacheHierarchy &caches() const { return caches_; }

private:
  void renameStage(u64 cycle);
  void issueStage(u64 cycle);
  void completeStage(u64 cycle);
  void commitStage(u64 cycle);
  void recoverFromBranch(const RobEntry &entry, u32 actualTarget);
  void completeInstruction(RobEntry &entry, u64 cycle);
  bool executeInstruction(RobEntry &entry, u64 cycle);
  u32 loadValue(u32 address, Opcode op);
  void storeValue(u32 address, u32 value, int size);
  bool sourcesReady(const RobEntry &entry) const;

  SimulatorConfig config_;
  Memory memory_;
  LoadedProgram program_;
  Decoder decoder_;
  BranchPredictor predictor_;
  CacheHierarchy caches_;
  PhysicalRegisterFile prf_;
  RenameUnit rename_;
  ReorderBuffer rob_;
  IssueQueue issueQueue_;
  ExecutionUnits execution_;
  InstructionFetch fetch_;
  std::deque<DecodedInstruction> fetchQueue_;
  Stats stats_;
  u64 nextSeq_ = 1;
  bool stoppedFetching_ = false;
};

} // namespace sim
