# Cycle-Accurate RV32I Out-of-Order Simulator

This repository contains a from-scratch C++17 simulator for a simplified out-of-order RV32I CPU. It models a cycle-stepped frontend, register renaming with a physical register file, age-ordered issue queue, ALU/load-store/branch execution units, a reorder buffer with in-order commit, a two-level branch predictor with BTB, and a configurable L1I/L1D/L2 cache hierarchy.

The simulator targets bare-metal RV32I ELF microbenchmarks. It does not emulate Linux; programs terminate with an `ecall` using `a7=93` and the exit code in `a0`.

## Build

```bash
make
make test
```

To build the included RISC-V workloads, install a freestanding RV32I toolchain and run:

```bash
make workloads
```

If your compiler prefix differs:

```bash
make workloads RISCV_PREFIX=riscv64-unknown-elf-
```

The workload flags are `-march=rv32i -mabi=ilp32 -nostdlib -ffreestanding`.

## Run

```bash
./sim --program workloads/branch.elf --config configs/default.json --stats out.json
```

The simulator prints a short completion line and writes detailed JSON stats to the requested path. A nonzero simulated exit code is returned as the process exit code.

## Showcase Demo

Open `web/index.html` in a browser for a polished interactive showcase of the simulator. The demo is static and shareable: it visualizes representative runs, pipeline flow, branch/cache metrics, ROB/RS occupancy, and instruction timelines without requiring a RISC-V toolchain or backend service.

The site is intended for portfolio use and can be published with GitHub Pages, Netlify, Vercel, or any static file host.

## Configuration

`configs/default.json` controls:

- Pipeline widths: fetch, decode, rename, issue, commit
- Core resources: ROB entries, issue queue entries, physical registers
- Execution resources: ALU, branch, and load/store unit counts and latencies
- Branch predictor: `gshare` or `bimodal`, history bits, counter table entries, BTB entries and associativity
- Cache hierarchy: L1I, L1D, L2 size, line size, associativity, latency, and `LRU` or `PLRU` replacement
- Simulation limit and trace enablement

The parser intentionally supports the simple JSON shape used by the config file and keeps the project free of external dependencies.

## Stats JSON

The output JSON includes:

- Summary: cycles, fetched and committed instructions, IPC, halted state, exit code
- Branch predictor: prediction count, accuracy, mispredictions, direction and target misses, BTB hits/misses
- Caches: accesses, hits, misses, hit rate, evictions, writebacks, average access latency for L1I, L1D, and L2
- Occupancy: ROB and reservation station min/max/average samples
- Stalls: named counters for frontend, rename, issue, commit, and branch recovery stalls
- Instruction trace: sequence number, PC, raw instruction, mnemonic, fetch/rename/issue/complete/commit cycles, branch and cache outcomes

## Workloads

Included assembly workloads:

- `workloads/arithmetic.S`: integer dependency loop
- `workloads/branch.S`: alternating branch pattern
- `workloads/cache.S`: strided load/store cache exercise

They share `workloads/start.S` and `workloads/linker.ld`, which place code at `0x10000`, initialize a small stack, call `main`, and exit via the simulator ABI.

## Current Model Boundaries

- RV32I only. Compressed, atomics, multiply/divide, floating point, privileged instructions, and Linux syscalls are rejected or treated as unsupported.
- Caches are blocking. There are no MSHRs or nonblocking memory-level parallelism.
- Loads are conservatively held behind older unresolved stores. Store data is written to memory at commit.
- Branch recovery restores a rename checkpoint, flushes younger ROB/issue/execution/frontend state, and resumes at the resolved target.
