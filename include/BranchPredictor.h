#pragma once

#include "Common.h"

namespace sim {

struct BranchPrediction {
  bool taken = false;
  u32 target = 0;
  bool btbHit = false;
};

struct BranchPredictorStats {
  u64 predictions = 0;
  u64 correct = 0;
  u64 directionMispredictions = 0;
  u64 targetMispredictions = 0;
  u64 btbHits = 0;
  u64 btbMisses = 0;
};

class BranchPredictor {
public:
  explicit BranchPredictor(const BranchPredictorConfig &config);
  BranchPrediction predict(u32 pc) const;
  void update(u32 pc, bool predictedTaken, u32 predictedTarget, bool actualTaken,
              u32 actualTarget);
  const BranchPredictorStats &stats() const { return stats_; }

private:
  struct BtbLine {
    bool valid = false;
    u32 tag = 0;
    u32 target = 0;
    u64 age = 0;
  };

  std::size_t directionIndex(u32 pc) const;
  void updateBtb(u32 pc, u32 target);

  BranchPredictorConfig config_;
  mutable BranchPredictorStats stats_;
  std::vector<u8> counters_;
  u32 globalHistory_ = 0;
  std::vector<std::vector<BtbLine>> btbSets_;
  mutable u64 btbClock_ = 0;
};

} // namespace sim
