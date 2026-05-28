#pragma once

#include "Common.h"
#include <deque>

namespace sim {

struct RenameCheckpoint {
  std::vector<int> map;
  std::deque<int> freeList;
};

class RenameUnit {
public:
  explicit RenameUnit(int physicalRegisters = 96);
  int physForArch(int arch) const;
  bool canAllocate() const;
  int allocate(int arch);
  void freePhysical(int physical);
  RenameCheckpoint checkpoint() const;
  void restore(const RenameCheckpoint &checkpoint);
  const std::vector<int> &map() const { return map_; }

private:
  std::vector<int> map_;
  std::deque<int> freeList_;
};

} // namespace sim
