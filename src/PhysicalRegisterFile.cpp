#include "PhysicalRegisterFile.h"

namespace sim {

PhysicalRegisterFile::PhysicalRegisterFile(int count)
    : values_(static_cast<std::size_t>(count), 0), ready_(static_cast<std::size_t>(count), true) {}

u32 PhysicalRegisterFile::read(int physical) const { return values_.at(static_cast<std::size_t>(physical)); }

void PhysicalRegisterFile::write(int physical, u32 value) {
  if (physical == 0) {
    return;
  }
  values_.at(static_cast<std::size_t>(physical)) = value;
  ready_.at(static_cast<std::size_t>(physical)) = true;
}

bool PhysicalRegisterFile::ready(int physical) const {
  return physical == 0 || ready_.at(static_cast<std::size_t>(physical));
}

void PhysicalRegisterFile::markNotReady(int physical) {
  if (physical != 0) {
    ready_.at(static_cast<std::size_t>(physical)) = false;
  }
}

} // namespace sim
