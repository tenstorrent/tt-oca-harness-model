TEST = spi_ot_flash_sector_erase_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32
EXT_INCS = -I$(OCH_ROOT)/meta/registers/c

all: $(TEST).elf

include $(OCH_ROOT)/fw/sep/tests/common/common.mk


.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
