#include "BranchPredictor.h"
#include "CacheHierarchy.h"
#include "Decoder.h"
#include "ElfLoader.h"
#include "IssueQueue.h"
#include "Memory.h"
#include "PhysicalRegisterFile.h"
#include "RenameUnit.h"
#include "ReorderBuffer.h"

#include <cassert>
#include <fstream>
#include <iostream>

using namespace sim;

namespace {

void put16(std::vector<u8> &b, std::size_t off, u16 v) {
  b[off] = static_cast<u8>(v & 0xff);
  b[off + 1] = static_cast<u8>((v >> 8) & 0xff);
}

void put32(std::vector<u8> &b, std::size_t off, u32 v) {
  b[off] = static_cast<u8>(v & 0xff);
  b[off + 1] = static_cast<u8>((v >> 8) & 0xff);
  b[off + 2] = static_cast<u8>((v >> 16) & 0xff);
  b[off + 3] = static_cast<u8>((v >> 24) & 0xff);
}

void testDecoder() {
  Decoder d;
  auto addi = d.decode(0x1000, 0x00500093u);
  assert(addi.opcode == Opcode::Addi);
  assert(addi.rd == 1);
  assert(addi.rs1 == 0);
  assert(addi.imm == 5);

  auto beq = d.decode(0x1000, 0x00208463u);
  assert(beq.opcode == Opcode::Beq);
  assert(beq.rs1 == 1);
  assert(beq.rs2 == 2);
  assert(beq.imm == 8);
}

void testMemory() {
  Memory m;
  m.write32(0x100, 0xdeadbeefu);
  assert(m.read8(0x100) == 0xef);
  assert(m.read16(0x100) == 0xbeef);
  assert(m.read32(0x100) == 0xdeadbeefu);
}

void testBranchPredictor() {
  BranchPredictorConfig cfg;
  cfg.mode = "bimodal";
  cfg.bimodalEntries = 16;
  cfg.btbEntries = 8;
  cfg.btbAssociativity = 2;
  BranchPredictor bp(cfg);
  auto p0 = bp.predict(0x100);
  assert(!p0.taken);
  bp.update(0x100, p0.taken, p0.target, true, 0x200);
  bp.update(0x100, false, 0x104, true, 0x200);
  auto p1 = bp.predict(0x100);
  assert(p1.taken);
  assert(p1.target == 0x200);
}

void testCache() {
  CacheLevelConfig l1;
  l1.sizeBytes = 64;
  l1.lineBytes = 16;
  l1.associativity = 2;
  l1.latency = 1;
  CacheLevelConfig l2 = l1;
  l2.sizeBytes = 128;
  l2.latency = 5;
  CacheHierarchy caches(l1, l1, l2);
  auto miss = caches.accessData(0x1000, false);
  auto hit = caches.accessData(0x1004, false);
  assert(!miss.hit);
  assert(hit.hit);
  assert(caches.l1d().stats().accesses == 2);
  assert(caches.l1d().stats().hits == 1);
}

void testRenameRobIssue() {
  RenameUnit rename(40);
  PhysicalRegisterFile prf(40);
  ReorderBuffer rob(4);
  IssueQueue iq(4);
  Decoder d;

  DecodedInstruction inst = d.decode(0x1000, 0x00108093u);
  RobEntry entry;
  entry.seq = 1;
  entry.inst = inst;
  entry.src1Phys = rename.physForArch(inst.rs1);
  entry.oldDestPhys = rename.physForArch(inst.rd);
  entry.destPhys = rename.allocate(inst.rd);
  entry.hasDest = true;
  prf.markNotReady(entry.destPhys);
  rob.allocate(entry);
  iq.push(1);
  assert(rob.size() == 1);
  assert(iq.selectReady(rob, prf, 1).size() == 1);
  prf.write(entry.destPhys, 7);
  rename.freePhysical(entry.oldDestPhys);
}

void testElfLoader() {
  std::vector<u8> elf(0x100 + 12, 0);
  elf[0] = 0x7f;
  elf[1] = 'E';
  elf[2] = 'L';
  elf[3] = 'F';
  elf[4] = 1;
  elf[5] = 1;
  put16(elf, 16, 2);
  put16(elf, 18, 243);
  put32(elf, 24, 0x10000);
  put32(elf, 28, 52);
  put16(elf, 42, 32);
  put16(elf, 44, 1);
  put32(elf, 52, 1);
  put32(elf, 56, 0x100);
  put32(elf, 60, 0x10000);
  put32(elf, 68, 12);
  put32(elf, 72, 12);
  put32(elf, 0x100, 0x00000513u);
  put32(elf, 0x104, 0x05d00893u);
  put32(elf, 0x108, 0x00000073u);

  const std::string path = "tests/minimal_test.elf";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(elf.data()), static_cast<std::streamsize>(elf.size()));
  }
  Memory memory;
  LoadedProgram program = ElfLoader().load(path, memory);
  assert(program.entry == 0x10000);
  assert(memory.read32(0x10000) == 0x00000513u);
  assert(memory.read32(0x10008) == 0x00000073u);
}

} // namespace

int main() {
  testDecoder();
  testMemory();
  testBranchPredictor();
  testCache();
  testRenameRobIssue();
  testElfLoader();
  std::cout << "unit tests passed\n";
  return 0;
}
