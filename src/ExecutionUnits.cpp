#include "ExecutionUnits.h"

#include <algorithm>

namespace sim {

ExecutionUnits::ExecutionUnits(int aluUnits, int branchUnits, int loadStoreUnits, int aluLatency,
                               int branchLatency, int loadStoreBaseLatency)
    : aluUnits_(aluUnits), branchUnits_(branchUnits), loadStoreUnits_(loadStoreUnits),
      aluLatency_(aluLatency), branchLatency_(branchLatency),
      loadStoreBaseLatency_(loadStoreBaseLatency) {}

int ExecutionUnits::capacityFor(const DecodedInstruction &inst) const {
  if (inst.opClass == OpClass::Branch || inst.opClass == OpClass::Jump) {
    return branchUnits_;
  }
  if (inst.opClass == OpClass::Load || inst.opClass == OpClass::Store) {
    return loadStoreUnits_;
  }
  return aluUnits_;
}

int ExecutionUnits::activeFor(const DecodedInstruction &inst) const {
  if (inst.opClass == OpClass::Branch || inst.opClass == OpClass::Jump) {
    return static_cast<int>(branch_.size());
  }
  if (inst.opClass == OpClass::Load || inst.opClass == OpClass::Store) {
    return static_cast<int>(loadStore_.size());
  }
  return static_cast<int>(alu_.size());
}

bool ExecutionUnits::canAccept(const DecodedInstruction &inst) const {
  return activeFor(inst) < capacityFor(inst);
}

int ExecutionUnits::latencyFor(const DecodedInstruction &inst) const {
  if (inst.opClass == OpClass::Branch || inst.opClass == OpClass::Jump) {
    return branchLatency_;
  }
  if (inst.opClass == OpClass::Load || inst.opClass == OpClass::Store) {
    return loadStoreBaseLatency_;
  }
  return aluLatency_;
}

void ExecutionUnits::start(const DecodedInstruction &inst, u64 seq, int extraLatency) {
  ActiveExecution active{seq, std::max(1, latencyFor(inst) + extraLatency)};
  if (inst.opClass == OpClass::Branch || inst.opClass == OpClass::Jump) {
    branch_.push_back(active);
  } else if (inst.opClass == OpClass::Load || inst.opClass == OpClass::Store) {
    loadStore_.push_back(active);
  } else {
    alu_.push_back(active);
  }
}

std::vector<u64> ExecutionUnits::tick() {
  std::vector<u64> completed;
  auto tickList = [&completed](std::vector<ActiveExecution> &list) {
    for (auto &entry : list) {
      --entry.remaining;
    }
    auto it = list.begin();
    while (it != list.end()) {
      if (it->remaining <= 0) {
        completed.push_back(it->seq);
        it = list.erase(it);
      } else {
        ++it;
      }
    }
  };
  tickList(alu_);
  tickList(branch_);
  tickList(loadStore_);
  std::sort(completed.begin(), completed.end());
  return completed;
}

void ExecutionUnits::flushYoungerThan(u64 seq) {
  auto flush = [seq](std::vector<ActiveExecution> &list) {
    list.erase(std::remove_if(list.begin(), list.end(),
                              [seq](const ActiveExecution &e) { return e.seq > seq; }),
               list.end());
  };
  flush(alu_);
  flush(branch_);
  flush(loadStore_);
}

} // namespace sim
