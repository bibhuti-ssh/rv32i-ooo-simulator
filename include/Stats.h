#pragma once

#include "BranchPredictor.h"
#include "CacheHierarchy.h"
#include "Common.h"
#include <map>

namespace sim {

struct OccupancyStats {
  u64 samples = 0;
  u64 total = 0;
  std::size_t min = std::numeric_limits<std::size_t>::max();
  std::size_t max = 0;
  void sample(std::size_t value);
  double average() const;
};

struct InstructionTrace {
  u64 seq = 0;
  u32 pc = 0;
  u32 raw = 0;
  std::string mnemonic;
  u64 fetchCycle = 0;
  u64 renameCycle = 0;
  u64 issueCycle = 0;
  u64 completeCycle = 0;
  u64 commitCycle = 0;
  bool predictedTaken = false;
  bool actualTaken = false;
  bool branchMispredicted = false;
  std::string cacheLevel;
  bool cacheHit = false;
};

struct Stats {
  u64 cycles = 0;
  u64 fetched = 0;
  u64 committed = 0;
  u64 retiredBranches = 0;
  u64 branchMispredictions = 0;
  u64 exitCode = 0;
  bool halted = false;
  std::map<std::string, u64> stalls;
  OccupancyStats robOccupancy;
  OccupancyStats rsOccupancy;
  std::vector<InstructionTrace> instructions;

  InstructionTrace &createTrace(u64 seq, const DecodedInstruction &inst, u64 cycle);
  InstructionTrace *traceFor(u64 seq);
  void writeJson(const std::string &path, const BranchPredictorStats &bp,
                 const CacheStats &l1i, const CacheStats &l1d,
                 const CacheStats &l2) const;
};

} // namespace sim
