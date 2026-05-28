#include "CacheHierarchy.h"

#include <algorithm>
#include <cctype>

namespace sim {
namespace {

bool isPlru(const std::string &policy) {
  std::string p = policy;
  std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return p == "PLRU";
}

} // namespace

CacheLevel::CacheLevel(std::string name, CacheLevelConfig config, CacheLevel *next)
    : name_(std::move(name)), config_(std::move(config)), next_(next) {
  if (config_.lineBytes == 0 || config_.associativity == 0 ||
      config_.sizeBytes < config_.lineBytes * config_.associativity) {
    throw SimError("invalid cache configuration for " + name_);
  }
  sets_ = config_.sizeBytes / (config_.lineBytes * config_.associativity);
  sets_ = std::max<std::size_t>(1, sets_);
  lines_.assign(sets_, std::vector<Line>(config_.associativity));
  plruBits_.assign(sets_, std::vector<u8>(std::max<std::size_t>(1, config_.associativity - 1)));
}

std::size_t CacheLevel::setIndex(u32 address) const {
  return (address / config_.lineBytes) % sets_;
}

u32 CacheLevel::tag(u32 address) const {
  return static_cast<u32>((address / config_.lineBytes) / sets_);
}

CacheAccessResult CacheLevel::access(u32 address, bool write) {
  ++stats_.accesses;
  ++clock_;
  const std::size_t set = setIndex(address);
  const u32 wantedTag = tag(address);
  for (std::size_t way = 0; way < lines_[set].size(); ++way) {
    auto &line = lines_[set][way];
    if (line.valid && line.tag == wantedTag) {
      ++stats_.hits;
      stats_.totalLatency += config_.latency;
      line.dirty = line.dirty || write;
      touch(set, static_cast<int>(way));
      return {config_.latency, true, name_};
    }
  }

  ++stats_.misses;
  CacheAccessResult lower;
  if (next_) {
    lower = next_->access(address, false);
  } else {
    lower.latency = 50;
    lower.level = "memory";
  }

  const int victimWay = chooseVictim(set);
  auto &victim = lines_[set][victimWay];
  if (victim.valid) {
    ++stats_.evictions;
    if (victim.dirty) {
      ++stats_.writebacks;
      if (next_) {
        const u32 victimAddress =
            ((victim.tag * static_cast<u32>(sets_)) + static_cast<u32>(set)) *
            static_cast<u32>(config_.lineBytes);
        lower.latency += next_->access(victimAddress, true).latency;
      }
    }
  }
  victim.valid = true;
  victim.dirty = write;
  victim.tag = wantedTag;
  touch(set, victimWay);

  const int total = config_.latency + lower.latency;
  stats_.totalLatency += static_cast<u64>(total);
  return {total, false, lower.level.empty() ? name_ : lower.level};
}

int CacheLevel::chooseVictim(std::size_t set) {
  for (std::size_t way = 0; way < lines_[set].size(); ++way) {
    if (!lines_[set][way].valid) {
      return static_cast<int>(way);
    }
  }
  if (isPlru(config_.replacement) && (config_.associativity & (config_.associativity - 1)) == 0) {
    return plruVictim(set);
  }
  auto it = std::min_element(lines_[set].begin(), lines_[set].end(),
                             [](const Line &a, const Line &b) { return a.age < b.age; });
  return static_cast<int>(std::distance(lines_[set].begin(), it));
}

void CacheLevel::touch(std::size_t set, int way) {
  lines_[set][way].age = clock_;
  if (isPlru(config_.replacement) && (config_.associativity & (config_.associativity - 1)) == 0) {
    plruTouch(set, way);
  }
}

void CacheLevel::plruTouch(std::size_t set, int way) {
  std::size_t node = 0;
  std::size_t span = config_.associativity;
  std::size_t offset = static_cast<std::size_t>(way);
  while (span > 1) {
    const bool right = offset >= span / 2;
    plruBits_[set][node] = right ? 0 : 1;
    node = node * 2 + (right ? 2 : 1);
    if (right) {
      offset -= span / 2;
    }
    span /= 2;
  }
}

int CacheLevel::plruVictim(std::size_t set) const {
  std::size_t node = 0;
  std::size_t way = 0;
  std::size_t span = config_.associativity;
  while (span > 1) {
    const bool goRight = plruBits_[set][node] != 0;
    node = node * 2 + (goRight ? 2 : 1);
    if (goRight) {
      way += span / 2;
    }
    span /= 2;
  }
  return static_cast<int>(way);
}

CacheHierarchy::CacheHierarchy(const CacheLevelConfig &l1i, const CacheLevelConfig &l1d,
                               const CacheLevelConfig &l2)
    : l2_("L2", l2, nullptr), l1i_("L1I", l1i, &l2_), l1d_("L1D", l1d, &l2_) {}

CacheAccessResult CacheHierarchy::accessInstruction(u32 address) {
  return l1i_.access(address, false);
}

CacheAccessResult CacheHierarchy::accessData(u32 address, bool write) {
  return l1d_.access(address, write);
}

} // namespace sim
