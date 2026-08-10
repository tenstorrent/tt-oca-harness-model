TEST = wdt_cdc_sync_test
export TEST

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

PICOLIBC_LIB = $(PICOLIBC_DIR)/picolibc/$(GCC_PREFIX)/lib/rv32imac/ilp32
EXT_LD_FLAGS = -L$(PICOLIBC_LIB) --oslib=dummyhost

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: wdt_cdc_sync_test.elf
