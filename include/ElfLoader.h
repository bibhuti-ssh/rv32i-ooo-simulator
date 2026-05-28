#pragma once

#include "Common.h"
#include "Memory.h"

namespace sim {

struct LoadedProgram {
  u32 entry = 0;
  u32 lowAddress = 0;
  u32 highAddress = 0;
};

class ElfLoader {
public:
  LoadedProgram load(const std::string &path, Memory &memory) const;
};

} // namespace sim
