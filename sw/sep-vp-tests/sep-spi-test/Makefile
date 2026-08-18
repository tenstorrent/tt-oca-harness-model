# SPI Controller Test Makefile

SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = main
LDFLAGS += -T ../common/link.ld

include ../Makefile.common

# --- Dedicated VP config ----------------------------------------------------
# The read checks expect erased flash (0xff) and program their own patterns, but
# the flash model can only clear bits, so the bootcode preload leaves
# `pattern & boot_byte` behind and the comparisons see boot image bytes. Drop the
# preload (see NO_SPIPRELOAD_INI in ../Makefile.common); no fixture is needed
# because this test stages everything it reads back itself.
VP_FLAGS := $(NO_SPIPRELOAD_INI)

# Give `sim` (from Makefile.common) the generated config as a prerequisite so a
# plain `make sim` works like every other test here.
sim: $(NO_SPIPRELOAD_INI)
