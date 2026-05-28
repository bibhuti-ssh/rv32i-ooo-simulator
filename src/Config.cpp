#include "Common.h"

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace sim {
namespace {

std::string readFile(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    throw SimError("unable to open config: " + path);
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string objectFor(const std::string &json, const std::string &key) {
  const std::string needle = "\"" + key + "\"";
  std::size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) {
    return "";
  }
  std::size_t brace = json.find('{', keyPos + needle.size());
  if (brace == std::string::npos) {
    return "";
  }
  int depth = 0;
  for (std::size_t i = brace; i < json.size(); ++i) {
    if (json[i] == '{') {
      ++depth;
    } else if (json[i] == '}') {
      --depth;
      if (depth == 0) {
        return json.substr(brace, i - brace + 1);
      }
    }
  }
  throw SimError("malformed JSON object for key: " + key);
}

long long getInt(const std::string &json, const std::string &key, long long fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
  std::smatch match;
  if (std::regex_search(json, match, pattern)) {
    return std::stoll(match[1].str());
  }
  return fallback;
}

bool getBool(const std::string &json, const std::string &key, bool fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
  std::smatch match;
  if (std::regex_search(json, match, pattern)) {
    return match[1].str() == "true";
  }
  return fallback;
}

std::string getString(const std::string &json, const std::string &key,
                      const std::string &fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
  std::smatch match;
  if (std::regex_search(json, match, pattern)) {
    return match[1].str();
  }
  return fallback;
}

CacheLevelConfig parseCache(const std::string &section, CacheLevelConfig cfg) {
  if (section.empty()) {
    return cfg;
  }
  cfg.sizeBytes = static_cast<std::size_t>(getInt(section, "sizeBytes", cfg.sizeBytes));
  cfg.lineBytes = static_cast<std::size_t>(getInt(section, "lineBytes", cfg.lineBytes));
  cfg.associativity =
      static_cast<std::size_t>(getInt(section, "associativity", cfg.associativity));
  cfg.latency = static_cast<int>(getInt(section, "latency", cfg.latency));
  cfg.replacement = getString(section, "replacement", cfg.replacement);
  return cfg;
}

} // namespace

SimulatorConfig SimulatorConfig::load(const std::string &path) {
  SimulatorConfig cfg;
  const std::string json = readFile(path);
  const std::string core = objectFor(json, "core");
  if (!core.empty()) {
    cfg.core.fetchWidth = static_cast<int>(getInt(core, "fetchWidth", cfg.core.fetchWidth));
    cfg.core.decodeWidth = static_cast<int>(getInt(core, "decodeWidth", cfg.core.decodeWidth));
    cfg.core.renameWidth = static_cast<int>(getInt(core, "renameWidth", cfg.core.renameWidth));
    cfg.core.issueWidth = static_cast<int>(getInt(core, "issueWidth", cfg.core.issueWidth));
    cfg.core.commitWidth = static_cast<int>(getInt(core, "commitWidth", cfg.core.commitWidth));
    cfg.core.robEntries =
        static_cast<std::size_t>(getInt(core, "robEntries", cfg.core.robEntries));
    cfg.core.issueQueueEntries =
        static_cast<std::size_t>(getInt(core, "issueQueueEntries", cfg.core.issueQueueEntries));
    cfg.core.physicalRegisters =
        static_cast<int>(getInt(core, "physicalRegisters", cfg.core.physicalRegisters));
    cfg.core.aluUnits = static_cast<int>(getInt(core, "aluUnits", cfg.core.aluUnits));
    cfg.core.branchUnits = static_cast<int>(getInt(core, "branchUnits", cfg.core.branchUnits));
    cfg.core.loadStoreUnits =
        static_cast<int>(getInt(core, "loadStoreUnits", cfg.core.loadStoreUnits));
    cfg.core.aluLatency = static_cast<int>(getInt(core, "aluLatency", cfg.core.aluLatency));
    cfg.core.branchLatency =
        static_cast<int>(getInt(core, "branchLatency", cfg.core.branchLatency));
    cfg.core.loadStoreBaseLatency = static_cast<int>(
        getInt(core, "loadStoreBaseLatency", cfg.core.loadStoreBaseLatency));
    cfg.core.maxCycles = static_cast<u64>(getInt(core, "maxCycles", cfg.core.maxCycles));
    cfg.core.trace = getBool(core, "trace", cfg.core.trace);
  }

  const std::string branch = objectFor(json, "branchPredictor");
  if (!branch.empty()) {
    cfg.branch.mode = getString(branch, "mode", cfg.branch.mode);
    cfg.branch.historyBits =
        static_cast<std::size_t>(getInt(branch, "historyBits", cfg.branch.historyBits));
    cfg.branch.bimodalEntries =
        static_cast<std::size_t>(getInt(branch, "bimodalEntries", cfg.branch.bimodalEntries));
    cfg.branch.btbEntries =
        static_cast<std::size_t>(getInt(branch, "btbEntries", cfg.branch.btbEntries));
    cfg.branch.btbAssociativity = static_cast<std::size_t>(
        getInt(branch, "btbAssociativity", cfg.branch.btbAssociativity));
  }

  cfg.l1i = parseCache(objectFor(json, "l1i"), cfg.l1i);
  cfg.l1d = parseCache(objectFor(json, "l1d"), cfg.l1d);
  CacheLevelConfig defaultL2;
  defaultL2.sizeBytes = 256 * 1024;
  defaultL2.lineBytes = 64;
  defaultL2.associativity = 8;
  defaultL2.latency = 12;
  defaultL2.replacement = "LRU";
  cfg.l2 = parseCache(objectFor(json, "l2"), defaultL2);
  return cfg;
}

} // namespace sim
