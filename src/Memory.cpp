#include "Memory.h"

namespace sim {

void Memory::write8(u32 address, u8 value) { bytes_[address] = value; }

void Memory::write(u32 address, const std::vector<u8> &bytes) {
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    write8(address + static_cast<u32>(i), bytes[i]);
  }
}

u8 Memory::read8(u32 address) const {
  auto it = bytes_.find(address);
  return it == bytes_.end() ? 0 : it->second;
}

u16 Memory::read16(u32 address) const {
  return static_cast<u16>(read8(address)) | (static_cast<u16>(read8(address + 1)) << 8);
}

u32 Memory::read32(u32 address) const {
  return static_cast<u32>(read8(address)) | (static_cast<u32>(read8(address + 1)) << 8) |
         (static_cast<u32>(read8(address + 2)) << 16) |
         (static_cast<u32>(read8(address + 3)) << 24);
}

void Memory::write16(u32 address, u16 value) {
  write8(address, static_cast<u8>(value & 0xff));
  write8(address + 1, static_cast<u8>((value >> 8) & 0xff));
}

void Memory::write32(u32 address, u32 value) {
  write8(address, static_cast<u8>(value & 0xff));
  write8(address + 1, static_cast<u8>((value >> 8) & 0xff));
  write8(address + 2, static_cast<u8>((value >> 16) & 0xff));
  write8(address + 3, static_cast<u8>((value >> 24) & 0xff));
}

bool Memory::hasByte(u32 address) const { return bytes_.find(address) != bytes_.end(); }

} // namespace sim
