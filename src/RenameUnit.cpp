#include "RenameUnit.h"

namespace sim {

RenameUnit::RenameUnit(int physicalRegisters) {
  if (physicalRegisters < kArchRegs) {
    throw SimError("physical register count must be at least 32");
  }
  map_.resize(kArchRegs);
  for (int i = 0; i < kArchRegs; ++i) {
    map_[i] = i;
  }
  for (int i = kArchRegs; i < physicalRegisters; ++i) {
    freeList_.push_back(i);
  }
}

int RenameUnit::physForArch(int arch) const {
  if (arch == 0) {
    return 0;
  }
  return map_.at(static_cast<std::size_t>(arch));
}

bool RenameUnit::canAllocate() const { return !freeList_.empty(); }

int RenameUnit::allocate(int arch) {
  if (arch == 0) {
    return 0;
  }
  if (freeList_.empty()) {
    throw SimError("physical register free list exhausted");
  }
  const int phys = freeList_.front();
  freeList_.pop_front();
  map_.at(static_cast<std::size_t>(arch)) = phys;
  return phys;
}

void RenameUnit::freePhysical(int physical) {
  if (physical >= kArchRegs) {
    freeList_.push_back(physical);
  }
}

RenameCheckpoint RenameUnit::checkpoint() const { return {map_, freeList_}; }

void RenameUnit::restore(const RenameCheckpoint &checkpoint) {
  map_ = checkpoint.map;
  freeList_ = checkpoint.freeList;
}

} // namespace sim
