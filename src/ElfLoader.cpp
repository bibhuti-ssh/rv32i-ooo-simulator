#include "ElfLoader.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace sim {
namespace {

u16 rd16(const std::vector<u8> &b, std::size_t off) {
  return static_cast<u16>(b.at(off)) | (static_cast<u16>(b.at(off + 1)) << 8);
}

u32 rd32(const std::vector<u8> &b, std::size_t off) {
  return static_cast<u32>(b.at(off)) | (static_cast<u32>(b.at(off + 1)) << 8) |
         (static_cast<u32>(b.at(off + 2)) << 16) | (static_cast<u32>(b.at(off + 3)) << 24);
}

} // namespace

LoadedProgram ElfLoader::load(const std::string &path, Memory &memory) const {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw SimError("unable to open program: " + path);
  }
  std::vector<u8> bytes((std::istreambuf_iterator<char>(in)), {});
  if (bytes.size() < 52 || bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' ||
      bytes[3] != 'F') {
    throw SimError("program is not an ELF file");
  }
  if (bytes[4] != 1 || bytes[5] != 1) {
    throw SimError("only little-endian ELF32 is supported");
  }
  const u16 type = rd16(bytes, 16);
  const u16 machine = rd16(bytes, 18);
  if (type != 2) {
    throw SimError("only executable bare-metal ELF files are supported");
  }
  if (machine != 243) {
    throw SimError("ELF machine is not RISC-V");
  }

  const u32 entry = rd32(bytes, 24);
  const u32 phoff = rd32(bytes, 28);
  const u16 phentsize = rd16(bytes, 42);
  const u16 phnum = rd16(bytes, 44);
  if (phentsize < 32) {
    throw SimError("unsupported ELF program header size");
  }

  LoadedProgram loaded;
  loaded.entry = entry;
  loaded.lowAddress = std::numeric_limits<u32>::max();
  loaded.highAddress = 0;

  for (u16 i = 0; i < phnum; ++i) {
    const std::size_t off = phoff + static_cast<std::size_t>(i) * phentsize;
    if (off + 32 > bytes.size()) {
      throw SimError("ELF program header outside file");
    }
    const u32 pType = rd32(bytes, off + 0);
    if (pType != 1) {
      continue;
    }
    const u32 pOffset = rd32(bytes, off + 4);
    const u32 pVaddr = rd32(bytes, off + 8);
    const u32 pFilesz = rd32(bytes, off + 16);
    const u32 pMemsz = rd32(bytes, off + 20);
    if (static_cast<std::size_t>(pOffset) + pFilesz > bytes.size()) {
      throw SimError("ELF segment outside file");
    }
    for (u32 j = 0; j < pFilesz; ++j) {
      memory.write8(pVaddr + j, bytes[pOffset + j]);
    }
    for (u32 j = pFilesz; j < pMemsz; ++j) {
      memory.write8(pVaddr + j, 0);
    }
    loaded.lowAddress = std::min(loaded.lowAddress, pVaddr);
    loaded.highAddress = std::max(loaded.highAddress, pVaddr + pMemsz);
  }

  if (loaded.lowAddress == std::numeric_limits<u32>::max()) {
    throw SimError("ELF contains no loadable segments");
  }
  return loaded;
}

} // namespace sim
