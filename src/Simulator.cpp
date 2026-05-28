#include "Simulator.h"

#include <algorithm>
#include <iostream>

namespace sim {
namespace {

u32 asBool(bool v) { return v ? 1u : 0u; }

bool signedLess(u32 a, u32 b) {
  return static_cast<i32>(a) < static_cast<i32>(b);
}

bool signedGreaterEqual(u32 a, u32 b) {
  return static_cast<i32>(a) >= static_cast<i32>(b);
}

} // namespace

Simulator::Simulator(SimulatorConfig config, std::string programPath)
    : config_(std::move(config)), predictor_(config_.branch),
      caches_(config_.l1i, config_.l1d, config_.l2),
      prf_(config_.core.physicalRegisters), rename_(config_.core.physicalRegisters),
      rob_(config_.core.robEntries), issueQueue_(config_.core.issueQueueEntries),
      execution_(config_.core.aluUnits, config_.core.branchUnits,
                 config_.core.loadStoreUnits, config_.core.aluLatency,
                 config_.core.branchLatency, config_.core.loadStoreBaseLatency),
      fetch_(0, config_.core.fetchWidth) {
  program_ = ElfLoader().load(programPath, memory_);
  fetch_.setPc(program_.entry);
  prf_.write(2, 0x80000000u);
}

const Stats &Simulator::run() {
  while (!stats_.halted && stats_.cycles < config_.core.maxCycles) {
    ++stats_.cycles;
    const u64 cycle = stats_.cycles;
    stats_.robOccupancy.sample(rob_.size());
    stats_.rsOccupancy.sample(issueQueue_.size());

    commitStage(cycle);

    for (u64 seq : execution_.tick()) {
      if (RobEntry *entry = rob_.find(seq)) {
        completeInstruction(*entry, cycle);
      }
    }

    issueStage(cycle);
    renameStage(cycle);

    if (!stoppedFetching_) {
      fetch_.tick(memory_, decoder_, predictor_, caches_, fetchQueue_, stats_, cycle);
    }
  }

  if (!stats_.halted) {
    throw SimError("simulation reached maxCycles before program exit");
  }
  return stats_;
}

void Simulator::renameStage(u64 cycle) {
  int renamed = 0;
  const int width = std::min(config_.core.renameWidth, config_.core.decodeWidth);
  while (renamed < width && !fetchQueue_.empty()) {
    DecodedInstruction inst = fetchQueue_.front();
    const bool hasDest = writesRegister(inst.opcode) && inst.rd != 0;
    if (rob_.full()) {
      ++stats_.stalls["rename_rob_full"];
      return;
    }
    if (issueQueue_.full()) {
      ++stats_.stalls["rename_issue_queue_full"];
      return;
    }
    if (hasDest && !rename_.canAllocate()) {
      ++stats_.stalls["rename_physical_registers"];
      return;
    }

    RobEntry entry;
    entry.seq = nextSeq_++;
    entry.inst = inst;
    entry.src1Phys = rename_.physForArch(inst.rs1);
    entry.src2Phys = rename_.physForArch(inst.rs2);
    entry.hasDest = hasDest;
    if (hasDest) {
      entry.oldDestPhys = rename_.physForArch(inst.rd);
      entry.destPhys = rename_.allocate(inst.rd);
      prf_.markNotReady(entry.destPhys);
    }
    if (inst.opClass == OpClass::Branch || inst.opClass == OpClass::Jump) {
      entry.checkpoint = rename_.checkpoint();
      entry.hasCheckpoint = true;
    }

    RobEntry &allocated = rob_.allocate(std::move(entry));
    issueQueue_.push(allocated.seq);
    InstructionTrace &trace = stats_.createTrace(allocated.seq, allocated.inst, cycle);
    trace.renameCycle = cycle;
    fetchQueue_.pop_front();
    if (allocated.inst.opcode == Opcode::Ecall || allocated.inst.opClass == OpClass::Illegal) {
      stoppedFetching_ = true;
      fetchQueue_.clear();
    }
    ++renamed;
  }
}

void Simulator::issueStage(u64 cycle) {
  const std::vector<u64> ready = issueQueue_.selectReady(rob_, prf_, config_.core.issueWidth);
  for (u64 seq : ready) {
    RobEntry *entry = rob_.find(seq);
    if (!entry || entry->issued) {
      continue;
    }
    if (!execution_.canAccept(entry->inst)) {
      ++stats_.stalls["issue_fu_busy"];
      continue;
    }

    int extraLatency = 0;
    if (isLoad(entry->inst.opcode) || isStore(entry->inst.opcode)) {
      const u32 base = prf_.read(entry->src1Phys);
      const u32 address = base + static_cast<u32>(entry->inst.imm);
      const int size = memorySizeBytes(entry->inst.opcode);
      if ((size == 2 && (address & 1u)) || (size == 4 && (address & 3u))) {
        entry->exception = true;
        entry->exceptionMessage = "misaligned memory access";
        entry->ready = true;
        issueQueue_.remove(seq);
        continue;
      }
      CacheAccessResult cache = caches_.accessData(address, isStore(entry->inst.opcode));
      extraLatency = std::max(0, cache.latency - 1);
      if (InstructionTrace *trace = stats_.traceFor(seq)) {
        trace->cacheLevel = cache.level;
        trace->cacheHit = cache.hit;
      }
    }

    entry->issued = true;
    issueQueue_.remove(seq);
    execution_.start(entry->inst, seq, extraLatency);
    if (InstructionTrace *trace = stats_.traceFor(seq)) {
      trace->issueCycle = cycle;
    }
  }
}

void Simulator::completeInstruction(RobEntry &entry, u64 cycle) {
  if (entry.ready) {
    return;
  }
  executeInstruction(entry, cycle);
  entry.ready = true;
  if (entry.hasDest) {
    prf_.write(entry.destPhys, entry.value);
  }
  if (InstructionTrace *trace = stats_.traceFor(entry.seq)) {
    trace->completeCycle = cycle;
  }
}

bool Simulator::executeInstruction(RobEntry &entry, u64 cycle) {
  const auto &inst = entry.inst;
  const u32 a = prf_.read(entry.src1Phys);
  const u32 b = prf_.read(entry.src2Phys);
  u32 value = 0;
  bool actualTaken = false;
  u32 actualTarget = inst.pc + 4;

  switch (inst.opcode) {
  case Opcode::Lui:
    value = static_cast<u32>(inst.imm);
    break;
  case Opcode::Auipc:
    value = inst.pc + static_cast<u32>(inst.imm);
    break;
  case Opcode::Jal:
    value = inst.pc + 4;
    actualTaken = true;
    actualTarget = inst.pc + static_cast<u32>(inst.imm);
    break;
  case Opcode::Jalr:
    value = inst.pc + 4;
    actualTaken = true;
    actualTarget = (a + static_cast<u32>(inst.imm)) & ~1u;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, true, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Beq:
    actualTaken = a == b;
    actualTarget = actualTaken ? inst.pc + static_cast<u32>(inst.imm) : inst.pc + 4;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, actualTaken, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Bne:
    actualTaken = a != b;
    actualTarget = actualTaken ? inst.pc + static_cast<u32>(inst.imm) : inst.pc + 4;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, actualTaken, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Blt:
    actualTaken = signedLess(a, b);
    actualTarget = actualTaken ? inst.pc + static_cast<u32>(inst.imm) : inst.pc + 4;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, actualTaken, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Bge:
    actualTaken = signedGreaterEqual(a, b);
    actualTarget = actualTaken ? inst.pc + static_cast<u32>(inst.imm) : inst.pc + 4;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, actualTaken, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Bltu:
    actualTaken = a < b;
    actualTarget = actualTaken ? inst.pc + static_cast<u32>(inst.imm) : inst.pc + 4;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, actualTaken, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Bgeu:
    actualTaken = a >= b;
    actualTarget = actualTaken ? inst.pc + static_cast<u32>(inst.imm) : inst.pc + 4;
    predictor_.update(inst.pc, inst.predictedTaken, inst.predictedTarget, actualTaken, actualTarget);
    ++stats_.retiredBranches;
    break;
  case Opcode::Lb:
  case Opcode::Lh:
  case Opcode::Lw:
  case Opcode::Lbu:
  case Opcode::Lhu:
    value = loadValue(a + static_cast<u32>(inst.imm), inst.opcode);
    break;
  case Opcode::Sb:
  case Opcode::Sh:
  case Opcode::Sw:
    entry.storeAddress = a + static_cast<u32>(inst.imm);
    entry.storeData = b;
    entry.storeSize = memorySizeBytes(inst.opcode);
    break;
  case Opcode::Addi:
    value = a + static_cast<u32>(inst.imm);
    break;
  case Opcode::Slti:
    value = asBool(static_cast<i32>(a) < inst.imm);
    break;
  case Opcode::Sltiu:
    value = asBool(a < static_cast<u32>(inst.imm));
    break;
  case Opcode::Xori:
    value = a ^ static_cast<u32>(inst.imm);
    break;
  case Opcode::Ori:
    value = a | static_cast<u32>(inst.imm);
    break;
  case Opcode::Andi:
    value = a & static_cast<u32>(inst.imm);
    break;
  case Opcode::Slli:
    value = a << (static_cast<u32>(inst.imm) & 0x1fu);
    break;
  case Opcode::Srli:
    value = a >> (static_cast<u32>(inst.imm) & 0x1fu);
    break;
  case Opcode::Srai:
    value = static_cast<u32>(static_cast<i32>(a) >> (static_cast<u32>(inst.imm) & 0x1fu));
    break;
  case Opcode::Add:
    value = a + b;
    break;
  case Opcode::Sub:
    value = a - b;
    break;
  case Opcode::Sll:
    value = a << (b & 0x1fu);
    break;
  case Opcode::Slt:
    value = asBool(static_cast<i32>(a) < static_cast<i32>(b));
    break;
  case Opcode::Sltu:
    value = asBool(a < b);
    break;
  case Opcode::Xor:
    value = a ^ b;
    break;
  case Opcode::Srl:
    value = a >> (b & 0x1fu);
    break;
  case Opcode::Sra:
    value = static_cast<u32>(static_cast<i32>(a) >> (b & 0x1fu));
    break;
  case Opcode::Or:
    value = a | b;
    break;
  case Opcode::And:
    value = a & b;
    break;
  case Opcode::Fence:
    break;
  case Opcode::Ecall:
    break;
  case Opcode::Illegal:
  default:
    entry.exception = true;
    entry.exceptionMessage = "illegal or unsupported RV32I instruction at pc " +
                             std::to_string(inst.pc);
    break;
  }

  entry.value = value;
  if (inst.opClass == OpClass::Branch || inst.opcode == Opcode::Jalr ||
      inst.opcode == Opcode::Jal) {
    const bool mispredicted =
        inst.predictedTaken != actualTaken ||
        (actualTaken && inst.predictedTarget != actualTarget);
    if (InstructionTrace *trace = stats_.traceFor(entry.seq)) {
      trace->actualTaken = actualTaken;
      trace->branchMispredicted = mispredicted;
    }
    if (mispredicted) {
      ++stats_.branchMispredictions;
      recoverFromBranch(entry, actualTarget);
    }
  }
  (void)cycle;
  return true;
}

void Simulator::commitStage(u64 cycle) {
  int committedThisCycle = 0;
  while (committedThisCycle < config_.core.commitWidth) {
    RobEntry *head = rob_.head();
    if (!head || !head->ready) {
      if (head) {
        ++stats_.stalls["commit_head_not_ready"];
      }
      return;
    }
    if (head->exception) {
      throw SimError(head->exceptionMessage);
    }

    if (isStore(head->inst.opcode)) {
      storeValue(head->storeAddress, head->storeData, head->storeSize);
    }
    if (head->inst.opcode == Opcode::Ecall) {
      const u32 syscall = prf_.read(rename_.physForArch(17));
      if (syscall != 93) {
        throw SimError("unsupported ecall ABI; expected a7=93 exit");
      }
      stats_.exitCode = prf_.read(rename_.physForArch(10));
      stats_.halted = true;
      stoppedFetching_ = true;
    }
    if (head->hasDest) {
      rename_.freePhysical(head->oldDestPhys);
    }
    if (InstructionTrace *trace = stats_.traceFor(head->seq)) {
      trace->commitCycle = cycle;
    }
    ++stats_.committed;
    rob_.popHead();
    ++committedThisCycle;
    if (stats_.halted) {
      return;
    }
  }
}

void Simulator::recoverFromBranch(const RobEntry &entry, u32 actualTarget) {
  if (!entry.hasCheckpoint) {
    return;
  }
  rename_.restore(entry.checkpoint);
  rob_.flushYoungerThan(entry.seq);
  issueQueue_.flushYoungerThan(entry.seq);
  execution_.flushYoungerThan(entry.seq);
  fetchQueue_.clear();
  fetch_.flush(actualTarget);
  ++stats_.stalls["branch_recoveries"];
}

u32 Simulator::loadValue(u32 address, Opcode op) {
  switch (op) {
  case Opcode::Lb:
    return static_cast<u32>(static_cast<i32>(static_cast<std::int8_t>(memory_.read8(address))));
  case Opcode::Lh:
    return static_cast<u32>(static_cast<i32>(static_cast<std::int16_t>(memory_.read16(address))));
  case Opcode::Lw:
    return memory_.read32(address);
  case Opcode::Lbu:
    return memory_.read8(address);
  case Opcode::Lhu:
    return memory_.read16(address);
  default:
    return 0;
  }
}

void Simulator::storeValue(u32 address, u32 value, int size) {
  if (size == 1) {
    memory_.write8(address, static_cast<u8>(value & 0xff));
  } else if (size == 2) {
    memory_.write16(address, static_cast<u16>(value & 0xffff));
  } else if (size == 4) {
    memory_.write32(address, value);
  } else {
    throw SimError("invalid store size");
  }
}

bool Simulator::sourcesReady(const RobEntry &entry) const {
  return (entry.inst.rs1 == 0 || prf_.ready(entry.src1Phys)) &&
         (entry.inst.rs2 == 0 || prf_.ready(entry.src2Phys));
}

} // namespace sim
