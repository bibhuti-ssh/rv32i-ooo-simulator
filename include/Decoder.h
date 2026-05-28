#pragma once

#include "Common.h"

namespace sim {

class Decoder {
public:
  DecodedInstruction decode(u32 pc, u32 raw) const;
};

} // namespace sim
