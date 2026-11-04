# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-rom-boot-test/rom.mk
#
# The SMC boot ROM image, from the smc-vp ROM-only boot test's source; built via
# `make -f rom.mk` from the top-level Makefile in this directory.
ROM_DIR = ../../smc-vp-tests/smc-rom-boot-test
SRCS    = $(ROM_DIR)/rom.S
OBJS    = rom.o
TARGET  = smu_rom_boot.elf
LDFLAGS += -T $(ROM_DIR)/rom.ld

include ../../smc-vp-tests/Makefile.common

all: smu_rom_boot.bin

rom.o: $(ROM_DIR)/rom.S
	$(CC) $(CFLAGS) -c -o $@ $<

smu_rom_boot.bin: $(TARGET)
	$(OBJCOPY) -O binary $< $@

clean: clean-bin
clean-bin:
	rm -f smu_rom_boot.bin
.PHONY: clean-bin
