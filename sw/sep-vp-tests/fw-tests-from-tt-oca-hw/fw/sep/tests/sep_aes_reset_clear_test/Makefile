TEST = sep_aes_reset_clear_test
export TEST

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

# Override: EL2 core does not support Zbs (bexti causes illegal instruction trap)
CC_ABI = $(ABI) -march=rv32imc_zicsr_zifencei

all: $(TEST).elf
