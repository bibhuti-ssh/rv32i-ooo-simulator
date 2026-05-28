#include "Stats.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace sim {
namespace {

std::string esc(const std::string &s) {
  std::ostringstream out;
  for (char c : s) {
    switch (c) {
    case '"':
      out << "\\\"";
      break;
    case '\\':
      out << "\\\\";
      break;
    case '\n':
      out << "\\n";
      break;
    default:
      out << c;
      break;
    }
  }
  return out.str();
}

double rate(u64 numerator, u64 denominator) {
  return denominator == 0 ? 0.0 : static_cast<double>(numerator) / static_cast<double>(denominator);
}

void writeCache(std::ostream &out, const char *name, const CacheStats &stats, bool comma) {
  out << "    \"" << name << "\": {\n"
      << "      \"accesses\": " << stats.accesses << ",\n"
      << "      \"hits\": " << stats.hits << ",\n"
      << "      \"misses\": " << stats.misses << ",\n"
      << "      \"hitRate\": " << rate(stats.hits, stats.accesses) << ",\n"
      << "      \"evictions\": " << stats.evictions << ",\n"
      << "      \"writebacks\": " << stats.writebacks << ",\n"
      << "      \"averageLatency\": " << rate(stats.totalLatency, stats.accesses) << "\n"
      << "    }" << (comma ? "," : "") << "\n";
}

} // namespace

void OccupancyStats::sample(std::size_t value) {
  ++samples;
  total += value;
  min = std::min(min, value);
  max = std::max(max, value);
}

double OccupancyStats::average() const {
  return samples == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(samples);
}

InstructionTrace &Stats::createTrace(u64 seq, const DecodedInstruction &inst, u64 cycle) {
  instructions.push_back({});
  auto &trace = instructions.back();
  trace.seq = seq;
  trace.pc = inst.pc;
  trace.raw = inst.raw;
  trace.mnemonic = inst.mnemonic;
  trace.fetchCycle = inst.fetchCycle == 0 ? cycle : inst.fetchCycle;
  trace.predictedTaken = inst.predictedTaken;
  return trace;
}

InstructionTrace *Stats::traceFor(u64 seq) {
  for (auto &trace : instructions) {
    if (trace.seq == seq) {
      return &trace;
    }
  }
  return nullptr;
}

void Stats::writeJson(const std::string &path, const BranchPredictorStats &bp,
                      const CacheStats &l1i, const CacheStats &l1d,
                      const CacheStats &l2) const {
  std::ofstream out(path);
  if (!out) {
    throw SimError("unable to write stats JSON: " + path);
  }
  out << std::fixed << std::setprecision(6);
  out << "{\n";
  out << "  \"summary\": {\n"
      << "    \"cycles\": " << cycles << ",\n"
      << "    \"fetchedInstructions\": " << fetched << ",\n"
      << "    \"committedInstructions\": " << committed << ",\n"
      << "    \"ipc\": " << rate(committed, cycles) << ",\n"
      << "    \"halted\": " << (halted ? "true" : "false") << ",\n"
      << "    \"exitCode\": " << exitCode << "\n"
      << "  },\n";
  out << "  \"branchPredictor\": {\n"
      << "    \"predictions\": " << bp.predictions << ",\n"
      << "    \"correct\": " << bp.correct << ",\n"
      << "    \"accuracy\": " << rate(bp.correct, bp.predictions) << ",\n"
      << "    \"retiredBranches\": " << retiredBranches << ",\n"
      << "    \"mispredictions\": " << branchMispredictions << ",\n"
      << "    \"mispredictionRate\": " << rate(branchMispredictions, retiredBranches) << ",\n"
      << "    \"directionMispredictions\": " << bp.directionMispredictions << ",\n"
      << "    \"targetMispredictions\": " << bp.targetMispredictions << ",\n"
      << "    \"btbHits\": " << bp.btbHits << ",\n"
      << "    \"btbMisses\": " << bp.btbMisses << "\n"
      << "  },\n";
  out << "  \"caches\": {\n";
  writeCache(out, "l1i", l1i, true);
  writeCache(out, "l1d", l1d, true);
  writeCache(out, "l2", l2, false);
  out << "  },\n";
  out << "  \"occupancy\": {\n"
      << "    \"rob\": {\"min\": " << (robOccupancy.samples ? robOccupancy.min : 0)
      << ", \"max\": " << robOccupancy.max << ", \"average\": " << robOccupancy.average()
      << "},\n"
      << "    \"reservationStations\": {\"min\": "
      << (rsOccupancy.samples ? rsOccupancy.min : 0) << ", \"max\": " << rsOccupancy.max
      << ", \"average\": " << rsOccupancy.average() << "}\n"
      << "  },\n";
  out << "  \"stalls\": {";
  bool first = true;
  for (const auto &kv : stalls) {
    out << (first ? "" : ", ") << "\"" << esc(kv.first) << "\": " << kv.second;
    first = false;
  }
  out << "},\n";
  out << "  \"instructions\": [\n";
  for (std::size_t i = 0; i < instructions.size(); ++i) {
    const auto &t = instructions[i];
    out << "    {\"seq\": " << t.seq << ", \"pc\": " << t.pc << ", \"raw\": " << t.raw
        << ", \"mnemonic\": \"" << esc(t.mnemonic) << "\", \"fetch\": " << t.fetchCycle
        << ", \"rename\": " << t.renameCycle << ", \"issue\": " << t.issueCycle
        << ", \"complete\": " << t.completeCycle << ", \"commit\": " << t.commitCycle
        << ", \"predictedTaken\": " << (t.predictedTaken ? "true" : "false")
        << ", \"actualTaken\": " << (t.actualTaken ? "true" : "false")
        << ", \"branchMispredicted\": " << (t.branchMispredicted ? "true" : "false")
        << ", \"cacheLevel\": \"" << esc(t.cacheLevel) << "\", \"cacheHit\": "
        << (t.cacheHit ? "true" : "false") << "}" << (i + 1 == instructions.size() ? "" : ",")
        << "\n";
  }
  out << "  ]\n";
  out << "}\n";
}

} // namespace sim
