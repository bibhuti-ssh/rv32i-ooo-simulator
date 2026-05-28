#pragma once

#include "Common.h"
#include "PhysicalRegisterFile.h"
#include "ReorderBuffer.h"

namespace sim {

struct IssueEntry {
  u64 seq = 0;
};

class IssueQueue {
public:
  explicit IssueQueue(std::size_t capacity = 32);
  bool full() const { return entries_.size() >= capacity_; }
  std::size_t size() const { return entries_.size(); }
  std::size_t capacity() const { return capacity_; }
  void push(u64 seq);
  void remove(u64 seq);
  void flushYoungerThan(u64 seq);
  std::vector<u64> selectReady(const ReorderBuffer &rob, const PhysicalRegisterFile &prf,
                               int maxCount) const;

private:
  std::size_t capacity_;
  std::vector<IssueEntry> entries_;
};

} // namespace sim
