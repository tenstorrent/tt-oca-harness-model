# Makefile for SMC WDT platform test

SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = smc_wdt_test
LDFLAGS += -T ../common/link.ld
# Allow enough wall-clock sim time for the WDT counter to hit CMP.
SIM_TIME_MS ?= 200

include ../Makefile.common
