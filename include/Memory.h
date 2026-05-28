#pragma once

#include "Common.h"

namespace sim {

class Memory {
public:
  void write8(u32 address, u8 value);
  void write(u32 address, const std::vector<u8> &bytes);
  u8 read8(u32 address) const;
  u16 read16(u32 address) const;
  u32 read32(u32 address) const;
  void write16(u32 address, u16 value);
  void write32(u32 address, u32 value);
  bool hasByte(u32 address) const;

private:
  std::unordered_map<u32, u8> bytes_;
};

} // namespace sim
