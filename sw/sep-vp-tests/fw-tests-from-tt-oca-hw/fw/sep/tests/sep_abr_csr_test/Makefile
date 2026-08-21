TEST = sep_abr_csr_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

# Match EXT_C_FLAGS. xpack GCC 15 has no rv32imac multilib; linking with
# common.mk's default -march=rv32imac would miss picolibc and pull newlib.
LD_ABI = $(ABI) -march=rv32imc

all: $(TEST).elf

.PHONY: help
help:
	@echo "Adams Bridge CSR identity test: $(TEST)"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
