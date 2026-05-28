#pragma once

#include "Common.h"

namespace sim {

class PhysicalRegisterFile {
public:
  explicit PhysicalRegisterFile(int count = 96);
  u32 read(int physical) const;
  void write(int physical, u32 value);
  bool ready(int physical) const;
  void markNotReady(int physical);
  int count() const { return static_cast<int>(values_.size()); }

private:
  std::vector<u32> values_;
  std::vector<bool> ready_;
};

} // namespace sim
