#include "ReorderBuffer.h"

namespace sim {

ReorderBuffer::ReorderBuffer(std::size_t capacity) : capacity_(capacity) {}

bool ReorderBuffer::full() const { return entries_.size() >= capacity_; }

RobEntry &ReorderBuffer::allocate(RobEntry entry) {
  if (full()) {
    throw SimError("ROB is full");
  }
  entries_.push_back(std::move(entry));
  return entries_.back();
}

RobEntry *ReorderBuffer::find(u64 seq) {
  for (auto &entry : entries_) {
    if (entry.seq == seq) {
      return &entry;
    }
  }
  return nullptr;
}

const RobEntry *ReorderBuffer::find(u64 seq) const {
  for (const auto &entry : entries_) {
    if (entry.seq == seq) {
      return &entry;
    }
  }
  return nullptr;
}

RobEntry *ReorderBuffer::head() { return entries_.empty() ? nullptr : &entries_.front(); }

void ReorderBuffer::popHead() {
  if (!entries_.empty()) {
    entries_.pop_front();
  }
}

void ReorderBuffer::flushYoungerThan(u64 seq) {
  while (!entries_.empty() && entries_.back().seq > seq) {
    entries_.pop_back();
  }
}

bool ReorderBuffer::hasOlderUnreadyStore(u64 seq) const {
  for (const auto &entry : entries_) {
    if (entry.seq >= seq) {
      return false;
    }
    if (isStore(entry.inst.opcode) && !entry.ready) {
      return true;
    }
  }
  return false;
}

std::vector<u64> ReorderBuffer::sequenceNumbers() const {
  std::vector<u64> seqs;
  for (const auto &entry : entries_) {
    seqs.push_back(entry.seq);
  }
  return seqs;
}

} // namespace sim
