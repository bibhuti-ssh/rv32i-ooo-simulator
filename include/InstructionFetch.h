#pragma once

#include "BranchPredictor.h"
#include "CacheHierarchy.h"
#include "Common.h"
#include "Decoder.h"
#include "Memory.h"
#include "Stats.h"

namespace sim {

class InstructionFetch {
public:
  InstructionFetch(u32 startPc, int width);
  void setPc(u32 pc);
  u32 pc() const { return pc_; }
  void flush(u32 pc);
  bool tick(const Memory &memory, const Decoder &decoder, BranchPredictor &predictor,
            CacheHierarchy &caches, std::deque<DecodedInstruction> &fetchQueue,
            Stats &stats, u64 cycle, std::size_t maxQueue = 32);

private:
  u32 pc_;
  int width_;
  int waitCycles_ = 0;
};

} // namespace sim
