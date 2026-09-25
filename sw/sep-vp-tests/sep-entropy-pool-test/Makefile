# Files
SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = sep-entropy-pool-test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
