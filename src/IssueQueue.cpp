#include "IssueQueue.h"

#include <algorithm>

namespace sim {

IssueQueue::IssueQueue(std::size_t capacity) : capacity_(capacity) {}

void IssueQueue::push(u64 seq) {
  if (full()) {
    throw SimError("issue queue is full");
  }
  entries_.push_back({seq});
}

void IssueQueue::remove(u64 seq) {
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                [seq](const IssueEntry &entry) { return entry.seq == seq; }),
                 entries_.end());
}

void IssueQueue::flushYoungerThan(u64 seq) {
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                [seq](const IssueEntry &entry) { return entry.seq > seq; }),
                 entries_.end());
}

std::vector<u64> IssueQueue::selectReady(const ReorderBuffer &rob,
                                         const PhysicalRegisterFile &prf,
                                         int maxCount) const {
  std::vector<u64> selected;
  for (const auto &entry : entries_) {
    const RobEntry *robEntry = rob.find(entry.seq);
    if (!robEntry || robEntry->issued || robEntry->ready) {
      continue;
    }
    if (isLoad(robEntry->inst.opcode) && rob.hasOlderUnreadyStore(robEntry->seq)) {
      continue;
    }
    const bool src1Ready = robEntry->inst.rs1 == 0 || prf.ready(robEntry->src1Phys);
    const bool src2Ready = robEntry->inst.rs2 == 0 || prf.ready(robEntry->src2Phys);
    if (src1Ready && src2Ready) {
      selected.push_back(entry.seq);
      if (static_cast<int>(selected.size()) >= maxCount) {
        break;
      }
    }
  }
  return selected;
}

} // namespace sim
