TEST = dma_path_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

all: dma_path_test.elf

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

