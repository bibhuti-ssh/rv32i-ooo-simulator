#pragma once

#include <cstdint>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace sim {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

constexpr u32 kWordBytes = 4;
constexpr int kArchRegs = 32;
constexpr u64 kInvalidSeq = std::numeric_limits<u64>::max();

struct SimError : public std::runtime_error {
  explicit SimError(const std::string &message) : std::runtime_error(message) {}
};

enum class OpClass { Alu, Load, Store, Branch, Jump, System, Fence, Illegal };

enum class Opcode {
  Nop,
  Lui,
  Auipc,
  Jal,
  Jalr,
  Beq,
  Bne,
  Blt,
  Bge,
  Bltu,
  Bgeu,
  Lb,
  Lh,
  Lw,
  Lbu,
  Lhu,
  Sb,
  Sh,
  Sw,
  Addi,
  Slti,
  Sltiu,
  Xori,
  Ori,
  Andi,
  Slli,
  Srli,
  Srai,
  Add,
  Sub,
  Sll,
  Slt,
  Sltu,
  Xor,
  Srl,
  Sra,
  Or,
  And,
  Fence,
  Ecall,
  Illegal
};

struct DecodedInstruction {
  u32 pc = 0;
  u32 raw = 0;
  u64 fetchCycle = 0;
  Opcode opcode = Opcode::Illegal;
  OpClass opClass = OpClass::Illegal;
  int rd = 0;
  int rs1 = 0;
  int rs2 = 0;
  i32 imm = 0;
  std::string mnemonic = "illegal";
  bool predictedTaken = false;
  u32 predictedTarget = 0;
  bool hasPrediction = false;
};

struct CacheLevelConfig {
  std::size_t sizeBytes = 16 * 1024;
  std::size_t lineBytes = 64;
  std::size_t associativity = 4;
  int latency = 1;
  std::string replacement = "LRU";
};

struct BranchPredictorConfig {
  std::string mode = "gshare";
  std::size_t historyBits = 8;
  std::size_t bimodalEntries = 1024;
  std::size_t btbEntries = 256;
  std::size_t btbAssociativity = 4;
};

struct CoreConfig {
  int fetchWidth = 4;
  int decodeWidth = 4;
  int renameWidth = 4;
  int issueWidth = 4;
  int commitWidth = 4;
  std::size_t robEntries = 64;
  std::size_t issueQueueEntries = 32;
  int physicalRegisters = 96;
  int aluUnits = 2;
  int branchUnits = 1;
  int loadStoreUnits = 1;
  int aluLatency = 1;
  int branchLatency = 1;
  int loadStoreBaseLatency = 1;
  u64 maxCycles = 1000000;
  bool trace = true;
};

struct SimulatorConfig {
  CoreConfig core;
  BranchPredictorConfig branch;
  CacheLevelConfig l1i;
  CacheLevelConfig l1d;
  CacheLevelConfig l2;
  static SimulatorConfig load(const std::string &path);
};

inline u32 signExtend(u32 value, int bits) {
  const u32 mask = 1u << (bits - 1);
  return (value ^ mask) - mask;
}

inline u32 bitRange(u32 value, int hi, int lo) {
  const u32 width = static_cast<u32>(hi - lo + 1);
  return (value >> lo) & ((1u << width) - 1u);
}

inline bool writesRegister(Opcode op) {
  switch (op) {
  case Opcode::Lui:
  case Opcode::Auipc:
  case Opcode::Jal:
  case Opcode::Jalr:
  case Opcode::Lb:
  case Opcode::Lh:
  case Opcode::Lw:
  case Opcode::Lbu:
  case Opcode::Lhu:
  case Opcode::Addi:
  case Opcode::Slti:
  case Opcode::Sltiu:
  case Opcode::Xori:
  case Opcode::Ori:
  case Opcode::Andi:
  case Opcode::Slli:
  case Opcode::Srli:
  case Opcode::Srai:
  case Opcode::Add:
  case Opcode::Sub:
  case Opcode::Sll:
  case Opcode::Slt:
  case Opcode::Sltu:
  case Opcode::Xor:
  case Opcode::Srl:
  case Opcode::Sra:
  case Opcode::Or:
  case Opcode::And:
    return true;
  default:
    return false;
  }
}

inline bool isBranch(Opcode op) {
  return op == Opcode::Beq || op == Opcode::Bne || op == Opcode::Blt ||
         op == Opcode::Bge || op == Opcode::Bltu || op == Opcode::Bgeu;
}

inline bool isJump(Opcode op) { return op == Opcode::Jal || op == Opcode::Jalr; }

inline bool isLoad(Opcode op) {
  return op == Opcode::Lb || op == Opcode::Lh || op == Opcode::Lw ||
         op == Opcode::Lbu || op == Opcode::Lhu;
}

inline bool isStore(Opcode op) {
  return op == Opcode::Sb || op == Opcode::Sh || op == Opcode::Sw;
}

inline int memorySizeBytes(Opcode op) {
  switch (op) {
  case Opcode::Lb:
  case Opcode::Lbu:
  case Opcode::Sb:
    return 1;
  case Opcode::Lh:
  case Opcode::Lhu:
  case Opcode::Sh:
    return 2;
  case Opcode::Lw:
  case Opcode::Sw:
    return 4;
  default:
    return 0;
  }
}

} // namespace sim
