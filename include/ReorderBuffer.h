#pragma once

#include "Common.h"
#include "RenameUnit.h"

namespace sim {

struct RobEntry {
  u64 seq = 0;
  DecodedInstruction inst;
  int src1Phys = 0;
  int src2Phys = 0;
  int destPhys = -1;
  int oldDestPhys = -1;
  bool hasDest = false;
  bool ready = false;
  bool issued = false;
  bool exception = false;
  std::string exceptionMessage;
  u32 value = 0;
  u32 storeAddress = 0;
  u32 storeData = 0;
  int storeSize = 0;
  RenameCheckpoint checkpoint;
  bool hasCheckpoint = false;
};

class ReorderBuffer {
public:
  explicit ReorderBuffer(std::size_t capacity = 64);
  bool full() const;
  bool empty() const { return entries_.empty(); }
  std::size_t size() const { return entries_.size(); }
  std::size_t capacity() const { return capacity_; }
  RobEntry &allocate(RobEntry entry);
  RobEntry *find(u64 seq);
  const RobEntry *find(u64 seq) const;
  RobEntry *head();
  void popHead();
  void flushYoungerThan(u64 seq);
  bool hasOlderUnreadyStore(u64 seq) const;
  std::vector<u64> sequenceNumbers() const;

private:
  std::size_t capacity_;
  std::deque<RobEntry> entries_;
};

} // namespace sim
