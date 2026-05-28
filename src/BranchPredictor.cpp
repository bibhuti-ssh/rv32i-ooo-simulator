#include "BranchPredictor.h"

#include <algorithm>

namespace sim {

BranchPredictor::BranchPredictor(const BranchPredictorConfig &config) : config_(config) {
  const std::size_t entries = std::max<std::size_t>(2, config_.bimodalEntries);
  counters_.assign(entries, 1);
  const std::size_t assoc = std::max<std::size_t>(1, config_.btbAssociativity);
  const std::size_t sets = std::max<std::size_t>(1, config_.btbEntries / assoc);
  btbSets_.assign(sets, std::vector<BtbLine>(assoc));
}

std::size_t BranchPredictor::directionIndex(u32 pc) const {
  const std::size_t mask = counters_.size() - 1;
  if (config_.mode == "bimodal") {
    return (pc >> 2) % counters_.size();
  }
  const u32 historyMask =
      config_.historyBits >= 32 ? 0xffffffffu : ((1u << config_.historyBits) - 1u);
  return (((pc >> 2) ^ (globalHistory_ & historyMask)) & mask) % counters_.size();
}

BranchPrediction BranchPredictor::predict(u32 pc) const {
  BranchPrediction pred;
  const std::size_t idx = directionIndex(pc);
  pred.taken = counters_[idx] >= 2;
  const std::size_t set = ((pc >> 2) % btbSets_.size());
  const u32 tag = pc / static_cast<u32>(btbSets_.size() * 4);
  for (const auto &line : btbSets_[set]) {
    if (line.valid && line.tag == tag) {
      pred.btbHit = true;
      pred.target = line.target;
      ++stats_.btbHits;
      return pred;
    }
  }
  ++stats_.btbMisses;
  pred.target = pc + 4;
  return pred;
}

void BranchPredictor::update(u32 pc, bool predictedTaken, u32 predictedTarget,
                             bool actualTaken, u32 actualTarget) {
  ++stats_.predictions;
  if (predictedTaken == actualTaken &&
      (!actualTaken || predictedTarget == actualTarget)) {
    ++stats_.correct;
  }
  if (predictedTaken != actualTaken) {
    ++stats_.directionMispredictions;
  } else if (actualTaken && predictedTarget != actualTarget) {
    ++stats_.targetMispredictions;
  }

  const std::size_t idx = directionIndex(pc);
  if (actualTaken) {
    counters_[idx] = std::min<u8>(3, counters_[idx] + 1);
    updateBtb(pc, actualTarget);
  } else {
    counters_[idx] = counters_[idx] == 0 ? 0 : counters_[idx] - 1;
  }
  const u32 historyMask =
      config_.historyBits >= 32 ? 0xffffffffu : ((1u << config_.historyBits) - 1u);
  globalHistory_ = ((globalHistory_ << 1) | (actualTaken ? 1u : 0u)) & historyMask;
}

void BranchPredictor::updateBtb(u32 pc, u32 target) {
  const std::size_t set = ((pc >> 2) % btbSets_.size());
  const u32 tag = pc / static_cast<u32>(btbSets_.size() * 4);
  ++btbClock_;
  for (auto &line : btbSets_[set]) {
    if (line.valid && line.tag == tag) {
      line.target = target;
      line.age = btbClock_;
      return;
    }
  }
  auto victim = std::min_element(btbSets_[set].begin(), btbSets_[set].end(),
                                 [](const BtbLine &a, const BtbLine &b) {
                                   if (a.valid != b.valid) {
                                     return !a.valid;
                                   }
                                   return a.age < b.age;
                                 });
  victim->valid = true;
  victim->tag = tag;
  victim->target = target;
  victim->age = btbClock_;
}

} // namespace sim
