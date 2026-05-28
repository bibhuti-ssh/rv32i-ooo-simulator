#pragma once

#include "Common.h"

namespace sim {

struct CacheAccessResult {
  int latency = 0;
  bool hit = false;
  std::string level;
};

struct CacheStats {
  u64 accesses = 0;
  u64 hits = 0;
  u64 misses = 0;
  u64 evictions = 0;
  u64 writebacks = 0;
  u64 totalLatency = 0;
};

class CacheLevel {
public:
  CacheLevel() = default;
  CacheLevel(std::string name, CacheLevelConfig config, CacheLevel *next);
  CacheAccessResult access(u32 address, bool write);
  const CacheStats &stats() const { return stats_; }
  const std::string &name() const { return name_; }

private:
  struct Line {
    bool valid = false;
    bool dirty = false;
    u32 tag = 0;
    u64 age = 0;
  };

  std::size_t setIndex(u32 address) const;
  u32 tag(u32 address) const;
  int chooseVictim(std::size_t set);
  void touch(std::size_t set, int way);
  void plruTouch(std::size_t set, int way);
  int plruVictim(std::size_t set) const;

  std::string name_;
  CacheLevelConfig config_;
  CacheLevel *next_ = nullptr;
  std::size_t sets_ = 1;
  std::vector<std::vector<Line>> lines_;
  std::vector<std::vector<u8>> plruBits_;
  u64 clock_ = 0;
  CacheStats stats_;
};

class CacheHierarchy {
public:
  CacheHierarchy(const CacheLevelConfig &l1i, const CacheLevelConfig &l1d,
                 const CacheLevelConfig &l2);
  CacheAccessResult accessInstruction(u32 address);
  CacheAccessResult accessData(u32 address, bool write);
  const CacheLevel &l1i() const { return l1i_; }
  const CacheLevel &l1d() const { return l1d_; }
  const CacheLevel &l2() const { return l2_; }

private:
  CacheLevel l2_;
  CacheLevel l1i_;
  CacheLevel l1d_;
};

} // namespace sim
