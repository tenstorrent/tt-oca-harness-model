# SRAM Memory Technical Specification

# Overview

This document specifies the SRAM (Static Random-Access Memory) hardware IP functionality for virtual platform environments. The SRAM module provides a high-performance, byte-addressable memory subsystem implemented as a SystemC Transaction Level Model (TLM2.0).

## Features

- **Paged Memory Architecture**: Efficient sparse memory allocation using 4KB pages
- **Byte-Addressable Access**: Support for byte, half-word, word, and burst memory operations
- **TLM2.0 Compliance**: Standard blocking transport interface for virtual platform integration
- **Read/Write Support**: Full read-write capability with configurable read-only mode
- **ELF Loading**: Direct support for loading executable binaries via load_if interface
- **Memory Efficiency**: Only allocates physical memory for accessed pages (sparse allocation)
- **Debug Support**: Transport debug interface for debugger access without timing

## Description

The SRAM module implements a memory subsystem using a paged memory architecture, where memory is organized into 4KB pages that are allocated on-demand. This approach provides excellent memory efficiency for virtual platforms, as only the memory pages that are actually accessed need to be allocated in the host system.

The module supports:
- **Dynamic Size Configuration**: Memory size specified at instantiation time
- **Sparse Allocation**: Pages allocated only when first accessed
- **Zero-Initialized Pages**: All allocated pages initialized to zero
- **Read-Only Mode**: Optional read-only configuration for protected memory regions

## Key Characteristics

### Memory Organization
- **Page Size**: 4096 bytes (4KB)
- **Address Space**: Configurable at instantiation (typically 64KB - 128KB)
- **Allocation Strategy**: On-demand page allocation
- **Initial State**: Unallocated (returns 0 on reads to unallocated pages)

### Access Patterns
- **Read Operations**: Return zero for unallocated pages, actual data for allocated pages
- **Write Operations**: Allocate page if needed, then write data
- **Burst Transfers**: Supported across page boundaries
- **Unaligned Access**: Fully supported with automatic byte packing

### Performance
- **Latency**: Fixed 10ns per transaction (TLM timing annotation)
- **Throughput**: Limited only by TLM quantum and SystemC scheduler
- **DMI Support**: Direct Memory Interface for accelerated instruction fetch (disabled for paged memory)

## Integration Notes

The SRAM module is designed for integration into RISC-V virtual platforms and supports:
- Standard TLM2.0 target socket interface
- ELF binary loading via load_if interface
- Integration with system bus interconnects
- Syscall handler access for runtime operations

## Memory Efficiency Benefits

Traditional contiguous memory models allocate the entire memory space upfront, which can consume significant host memory for large address spaces. The paged memory architecture only allocates what is actually used:

- **Traditional Model**: 128KB address space = 128KB host memory
- **Paged Model**: 128KB address space with 8KB usage = 8KB host memory (2 pages)

This is particularly beneficial for:
- Virtual platforms with large address spaces
- Multi-core simulations with per-core memory
- Sparse memory usage patterns typical of embedded software

## Use Cases

1. **Program Memory**: Executable code storage in RISC-V virtual platforms
2. **Data Memory**: Read-write data and stack regions
3. **Scratchpad Memory**: Fast local memory for processors
4. **Boot ROM**: Read-only boot code (when configured in read-only mode)

## Related Components

- **PagedMemory**: Underlying page management and byte-level access implementation
- **ROM**: Read-only variant using the same paged memory architecture
- **ELFLoader**: Loads executable binaries into SRAM via load_if interface
- **SyscallHandler**: Runtime system call operations accessing SRAM data
