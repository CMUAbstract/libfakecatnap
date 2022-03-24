LIB = libfakecatnap

OBJECTS += \
  catnap.o \
  events.o \
  fifos.o \
  timers.o \
  checkpoint.o \
  hw.o \
  comp.o

DEPS += libcapybara:gcc

LIBFCN_EVENT_THRESHOLD = 2880

override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \
  -DLFCN_STARTER_THRESH=$(LIBFCN_EVENT_THRESHOLD)
  #-DGDB_INT_CFG

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)

