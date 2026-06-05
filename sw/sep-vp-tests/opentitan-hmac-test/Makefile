# Makefile for OpenTitan HMAC Test on SEP Platform

# Additional include paths (must be before Makefile.common)
EXTRA_CFLAGS = -I../opentitan-compat

# Files
SRCS = ../common/start.S main.c ../opentitan-compat/dif_hmac_sep.c ../opentitan-compat/uart_output.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = opentitan-hmac-test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
