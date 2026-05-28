CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?=

SRC := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
SIM_SRC := $(SRC) src/main.cpp
TEST_SRC := tests/unit_tests.cpp

RISCV_PREFIX ?= riscv32-unknown-elf-
RISCV_CC := $(RISCV_PREFIX)gcc
RISCV_OBJCOPY := $(RISCV_PREFIX)objcopy
RISCV_CFLAGS := -march=rv32i -mabi=ilp32 -nostdlib -ffreestanding -fno-builtin -O2

.PHONY: all clean test workloads

all: sim

sim: $(SIM_SRC) include/*.h
	$(CXX) $(CXXFLAGS) $(SIM_SRC) $(LDFLAGS) -o $@

unit_tests: $(SRC) $(TEST_SRC) include/*.h
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST_SRC) $(LDFLAGS) -o $@

test: unit_tests
	./unit_tests

workloads: workloads/arithmetic.elf workloads/branch.elf workloads/cache.elf

workloads/%.elf: workloads/%.S workloads/start.S workloads/linker.ld
	$(RISCV_CC) $(RISCV_CFLAGS) -T workloads/linker.ld workloads/start.S $< -o $@

clean:
	rm -f sim unit_tests stats.json out.json tests/minimal_test.elf workloads/*.elf workloads/*.o
