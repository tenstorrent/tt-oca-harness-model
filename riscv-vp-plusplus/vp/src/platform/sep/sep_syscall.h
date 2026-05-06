#pragma once

#include "core/rv32/syscall.h"
#include "paged_mem.h"

class SepSyscallHandler : public rv32::SyscallHandler {
public:
    PagedMemory& mem;

    SepSyscallHandler(sc_core::sc_module_name name, PagedMemory& m)
        : rv32::SyscallHandler(name), mem(m) {}

    uint8_t *guest_address_to_host_pointer(uintptr_t addr) override {
        return mem.getPtr(addr);
    }
};
