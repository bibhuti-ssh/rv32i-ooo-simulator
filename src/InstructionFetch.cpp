#include "InstructionFetch.h"

namespace sim {

InstructionFetch::InstructionFetch(u32 startPc, int width) : pc_(startPc), width_(width) {}

void InstructionFetch::setPc(u32 pc) { pc_ = pc; }

void InstructionFetch::flush(u32 pc) {
  pc_ = pc;
  waitCycles_ = 0;
}

bool InstructionFetch::tick(const Memory &memory, const Decoder &decoder,
                            BranchPredictor &predictor, CacheHierarchy &caches,
                            std::deque<DecodedInstruction> &fetchQueue, Stats &stats,
                            u64 cycle, std::size_t maxQueue) {
  if (waitCycles_ > 0) {
    --waitCycles_;
    ++stats.stalls["fetch_cache"];
    return false;
  }
  if (fetchQueue.size() >= maxQueue) {
    ++stats.stalls["fetch_queue_full"];
    return false;
  }

  bool fetchedAny = false;
  for (int i = 0; i < width_ && fetchQueue.size() < maxQueue; ++i) {
    CacheAccessResult cache = caches.accessInstruction(pc_);
    if (cache.latency > 1) {
      waitCycles_ = cache.latency - 1;
    }
    const u32 raw = memory.read32(pc_);
    DecodedInstruction inst = decoder.decode(pc_, raw);
    inst.fetchCycle = cycle;
    if (isBranch(inst.opcode) || inst.opcode == Opcode::Jalr) {
      BranchPrediction pred = predictor.predict(pc_);
      inst.predictedTaken = pred.taken;
      inst.predictedTarget = pred.taken ? pred.target : pc_ + 4;
      inst.hasPrediction = true;
      pc_ = inst.predictedTarget;
    } else if (inst.opcode == Opcode::Jal) {
      inst.predictedTaken = true;
      inst.predictedTarget = pc_ + static_cast<u32>(inst.imm);
      inst.hasPrediction = true;
      pc_ = inst.predictedTarget;
    } else {
      inst.predictedTaken = false;
      inst.predictedTarget = pc_ + 4;
      pc_ += 4;
    }
    fetchQueue.push_back(inst);
    ++stats.fetched;
    fetchedAny = true;
    if (cache.latency > 1 || inst.opClass == OpClass::Illegal || inst.opcode == Opcode::Ecall) {
      break;
    }
  }
  (void)cycle;
  return fetchedAny;
}

} // namespace sim
