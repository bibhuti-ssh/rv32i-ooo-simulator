#pragma once

#include "CacheHierarchy.h"
#include "Common.h"
#include "Memory.h"
#include "PhysicalRegisterFile.h"
#include "ReorderBuffer.h"
#include "Stats.h"
#include <functional>

namespace sim {

struct ExecutionResult {
  bool branchResolved = false;
  bool actualTaken = false;
  u32 actualTarget = 0;
  bool mispredicted = false;
  RenameCheckpoint recoveryCheckpoint;
};

struct ActiveExecution {
  u64 seq = 0;
  int remaining = 0;
};

class ExecutionUnits {
public:
  ExecutionUnits(int aluUnits, int branchUnits, int loadStoreUnits, int aluLatency,
                 int branchLatency, int loadStoreBaseLatency);
  bool canAccept(const DecodedInstruction &inst) const;
  void start(const DecodedInstruction &inst, u64 seq, int extraLatency);
  std::vector<u64> tick();
  void flushYoungerThan(u64 seq);
  int latencyFor(const DecodedInstruction &inst) const;

private:
  int capacityFor(const DecodedInstruction &inst) const;
  int activeFor(const DecodedInstruction &inst) const;

  int aluUnits_;
  int branchUnits_;
  int loadStoreUnits_;
  int aluLatency_;
  int branchLatency_;
  int loadStoreBaseLatency_;
  std::vector<ActiveExecution> alu_;
  std::vector<ActiveExecution> branch_;
  std::vector<ActiveExecution> loadStore_;
};

} // namespace sim
