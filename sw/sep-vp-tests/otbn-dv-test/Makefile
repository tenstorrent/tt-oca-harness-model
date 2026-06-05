# Files
SRCS = ../common/start.S main.c ../common/printf.c otbn_loops_otbn.c 
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = otbn-dv-test
LDFLAGS += -T ../common/link.ld


include ../Makefile.common
