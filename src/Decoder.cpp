#include "Decoder.h"

#include <sstream>

namespace sim {
namespace {

std::string reg(int r) { return "x" + std::to_string(r); }

DecodedInstruction make(u32 pc, u32 raw, Opcode op, OpClass cls, int rd, int rs1,
                        int rs2, i32 imm, const std::string &mnemonic) {
  DecodedInstruction inst;
  inst.pc = pc;
  inst.raw = raw;
  inst.opcode = op;
  inst.opClass = cls;
  inst.rd = rd;
  inst.rs1 = rs1;
  inst.rs2 = rs2;
  inst.imm = imm;
  inst.mnemonic = mnemonic;
  return inst;
}

std::string fmtI(const char *name, int rd, int rs1, i32 imm) {
  return std::string(name) + " " + reg(rd) + "," + reg(rs1) + "," + std::to_string(imm);
}

std::string fmtR(const char *name, int rd, int rs1, int rs2) {
  return std::string(name) + " " + reg(rd) + "," + reg(rs1) + "," + reg(rs2);
}

std::string fmtB(const char *name, int rs1, int rs2, i32 imm) {
  return std::string(name) + " " + reg(rs1) + "," + reg(rs2) + "," + std::to_string(imm);
}

std::string fmtMem(const char *name, int r, int base, i32 off) {
  return std::string(name) + " " + reg(r) + "," + std::to_string(off) + "(" + reg(base) + ")";
}

} // namespace

DecodedInstruction Decoder::decode(u32 pc, u32 raw) const {
  if ((raw & 0x3u) != 0x3u) {
    return make(pc, raw, Opcode::Illegal, OpClass::Illegal, 0, 0, 0, 0,
                "illegal compressed/non-32-bit");
  }

  const u32 opc = raw & 0x7f;
  const int rd = static_cast<int>(bitRange(raw, 11, 7));
  const int funct3 = static_cast<int>(bitRange(raw, 14, 12));
  const int rs1 = static_cast<int>(bitRange(raw, 19, 15));
  const int rs2 = static_cast<int>(bitRange(raw, 24, 20));
  const int funct7 = static_cast<int>(bitRange(raw, 31, 25));

  switch (opc) {
  case 0x37:
    return make(pc, raw, Opcode::Lui, OpClass::Alu, rd, 0, 0,
                static_cast<i32>(raw & 0xfffff000u), "lui " + reg(rd));
  case 0x17:
    return make(pc, raw, Opcode::Auipc, OpClass::Alu, rd, 0, 0,
                static_cast<i32>(raw & 0xfffff000u), "auipc " + reg(rd));
  case 0x6f: {
    u32 imm = (bitRange(raw, 31, 31) << 20) | (bitRange(raw, 19, 12) << 12) |
              (bitRange(raw, 20, 20) << 11) | (bitRange(raw, 30, 21) << 1);
    return make(pc, raw, Opcode::Jal, OpClass::Jump, rd, 0, 0,
                static_cast<i32>(signExtend(imm, 21)), "jal " + reg(rd));
  }
  case 0x67:
    if (funct3 == 0) {
      return make(pc, raw, Opcode::Jalr, OpClass::Jump, rd, rs1, 0,
                  static_cast<i32>(signExtend(bitRange(raw, 31, 20), 12)),
                  fmtI("jalr", rd, rs1, static_cast<i32>(signExtend(bitRange(raw, 31, 20), 12))));
    }
    break;
  case 0x63: {
    u32 imm = (bitRange(raw, 31, 31) << 12) | (bitRange(raw, 7, 7) << 11) |
              (bitRange(raw, 30, 25) << 5) | (bitRange(raw, 11, 8) << 1);
    const i32 simm = static_cast<i32>(signExtend(imm, 13));
    switch (funct3) {
    case 0:
      return make(pc, raw, Opcode::Beq, OpClass::Branch, 0, rs1, rs2, simm,
                  fmtB("beq", rs1, rs2, simm));
    case 1:
      return make(pc, raw, Opcode::Bne, OpClass::Branch, 0, rs1, rs2, simm,
                  fmtB("bne", rs1, rs2, simm));
    case 4:
      return make(pc, raw, Opcode::Blt, OpClass::Branch, 0, rs1, rs2, simm,
                  fmtB("blt", rs1, rs2, simm));
    case 5:
      return make(pc, raw, Opcode::Bge, OpClass::Branch, 0, rs1, rs2, simm,
                  fmtB("bge", rs1, rs2, simm));
    case 6:
      return make(pc, raw, Opcode::Bltu, OpClass::Branch, 0, rs1, rs2, simm,
                  fmtB("bltu", rs1, rs2, simm));
    case 7:
      return make(pc, raw, Opcode::Bgeu, OpClass::Branch, 0, rs1, rs2, simm,
                  fmtB("bgeu", rs1, rs2, simm));
    default:
      break;
    }
    break;
  }
  case 0x03: {
    const i32 imm = static_cast<i32>(signExtend(bitRange(raw, 31, 20), 12));
    switch (funct3) {
    case 0:
      return make(pc, raw, Opcode::Lb, OpClass::Load, rd, rs1, 0, imm, fmtMem("lb", rd, rs1, imm));
    case 1:
      return make(pc, raw, Opcode::Lh, OpClass::Load, rd, rs1, 0, imm, fmtMem("lh", rd, rs1, imm));
    case 2:
      return make(pc, raw, Opcode::Lw, OpClass::Load, rd, rs1, 0, imm, fmtMem("lw", rd, rs1, imm));
    case 4:
      return make(pc, raw, Opcode::Lbu, OpClass::Load, rd, rs1, 0, imm,
                  fmtMem("lbu", rd, rs1, imm));
    case 5:
      return make(pc, raw, Opcode::Lhu, OpClass::Load, rd, rs1, 0, imm,
                  fmtMem("lhu", rd, rs1, imm));
    default:
      break;
    }
    break;
  }
  case 0x23: {
    u32 imm = (bitRange(raw, 31, 25) << 5) | bitRange(raw, 11, 7);
    const i32 simm = static_cast<i32>(signExtend(imm, 12));
    switch (funct3) {
    case 0:
      return make(pc, raw, Opcode::Sb, OpClass::Store, 0, rs1, rs2, simm,
                  fmtMem("sb", rs2, rs1, simm));
    case 1:
      return make(pc, raw, Opcode::Sh, OpClass::Store, 0, rs1, rs2, simm,
                  fmtMem("sh", rs2, rs1, simm));
    case 2:
      return make(pc, raw, Opcode::Sw, OpClass::Store, 0, rs1, rs2, simm,
                  fmtMem("sw", rs2, rs1, simm));
    default:
      break;
    }
    break;
  }
  case 0x13: {
    const i32 imm = static_cast<i32>(signExtend(bitRange(raw, 31, 20), 12));
    switch (funct3) {
    case 0:
      return make(pc, raw, Opcode::Addi, OpClass::Alu, rd, rs1, 0, imm,
                  fmtI("addi", rd, rs1, imm));
    case 2:
      return make(pc, raw, Opcode::Slti, OpClass::Alu, rd, rs1, 0, imm,
                  fmtI("slti", rd, rs1, imm));
    case 3:
      return make(pc, raw, Opcode::Sltiu, OpClass::Alu, rd, rs1, 0, imm,
                  fmtI("sltiu", rd, rs1, imm));
    case 4:
      return make(pc, raw, Opcode::Xori, OpClass::Alu, rd, rs1, 0, imm,
                  fmtI("xori", rd, rs1, imm));
    case 6:
      return make(pc, raw, Opcode::Ori, OpClass::Alu, rd, rs1, 0, imm,
                  fmtI("ori", rd, rs1, imm));
    case 7:
      return make(pc, raw, Opcode::Andi, OpClass::Alu, rd, rs1, 0, imm,
                  fmtI("andi", rd, rs1, imm));
    case 1:
      if (funct7 == 0) {
        return make(pc, raw, Opcode::Slli, OpClass::Alu, rd, rs1, 0,
                    static_cast<i32>(rs2), fmtI("slli", rd, rs1, rs2));
      }
      break;
    case 5:
      if (funct7 == 0) {
        return make(pc, raw, Opcode::Srli, OpClass::Alu, rd, rs1, 0,
                    static_cast<i32>(rs2), fmtI("srli", rd, rs1, rs2));
      }
      if (funct7 == 0x20) {
        return make(pc, raw, Opcode::Srai, OpClass::Alu, rd, rs1, 0,
                    static_cast<i32>(rs2), fmtI("srai", rd, rs1, rs2));
      }
      break;
    default:
      break;
    }
    break;
  }
  case 0x33:
    if (funct7 == 0x00) {
      switch (funct3) {
      case 0:
        return make(pc, raw, Opcode::Add, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("add", rd, rs1, rs2));
      case 1:
        return make(pc, raw, Opcode::Sll, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("sll", rd, rs1, rs2));
      case 2:
        return make(pc, raw, Opcode::Slt, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("slt", rd, rs1, rs2));
      case 3:
        return make(pc, raw, Opcode::Sltu, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("sltu", rd, rs1, rs2));
      case 4:
        return make(pc, raw, Opcode::Xor, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("xor", rd, rs1, rs2));
      case 5:
        return make(pc, raw, Opcode::Srl, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("srl", rd, rs1, rs2));
      case 6:
        return make(pc, raw, Opcode::Or, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("or", rd, rs1, rs2));
      case 7:
        return make(pc, raw, Opcode::And, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("and", rd, rs1, rs2));
      default:
        break;
      }
    } else if (funct7 == 0x20) {
      if (funct3 == 0) {
        return make(pc, raw, Opcode::Sub, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("sub", rd, rs1, rs2));
      }
      if (funct3 == 5) {
        return make(pc, raw, Opcode::Sra, OpClass::Alu, rd, rs1, rs2, 0,
                    fmtR("sra", rd, rs1, rs2));
      }
    }
    break;
  case 0x0f:
    if (funct3 == 0) {
      return make(pc, raw, Opcode::Fence, OpClass::Fence, 0, 0, 0, 0, "fence");
    }
    break;
  case 0x73:
    if (raw == 0x00000073u) {
      return make(pc, raw, Opcode::Ecall, OpClass::System, 0, 0, 0, 0, "ecall");
    }
    break;
  default:
    break;
  }

  return make(pc, raw, Opcode::Illegal, OpClass::Illegal, 0, 0, 0, 0, "illegal");
}

} // namespace sim
