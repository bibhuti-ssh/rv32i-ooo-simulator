#include "Simulator.h"

#include <iostream>

namespace {

void usage() {
  std::cerr << "usage: ./sim --program program.elf --config configs/default.json --stats out.json\n";
}

} // namespace

int main(int argc, char **argv) {
  std::string program;
  std::string configPath = "configs/default.json";
  std::string statsPath = "stats.json";

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--program" && i + 1 < argc) {
      program = argv[++i];
    } else if (arg == "--config" && i + 1 < argc) {
      configPath = argv[++i];
    } else if (arg == "--stats" && i + 1 < argc) {
      statsPath = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      usage();
      return 0;
    } else {
      usage();
      return 2;
    }
  }

  if (program.empty()) {
    usage();
    return 2;
  }

  try {
    sim::SimulatorConfig config = sim::SimulatorConfig::load(configPath);
    sim::Simulator simulator(config, program);
    const sim::Stats &stats = simulator.run();
    stats.writeJson(statsPath, simulator.branchPredictor().stats(), simulator.caches().l1i().stats(),
                    simulator.caches().l1d().stats(), simulator.caches().l2().stats());
    std::cout << "simulation complete: " << stats.committed << " instructions, " << stats.cycles
              << " cycles, stats written to " << statsPath << "\n";
    return static_cast<int>(stats.exitCode == 0 ? 0 : stats.exitCode);
  } catch (const std::exception &e) {
    std::cerr << "sim error: " << e.what() << "\n";
    return 1;
  }
}
