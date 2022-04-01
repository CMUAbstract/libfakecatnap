LIB = libfakecatnap

OBJECTS += \
  catnap.o \
  events.o \
  fifos.o \
  timers.o \
  checkpoint.o \
  hw.o \
  comp.o \
  scheduler.o \
  culpeo.o

DEPS += libcapybara:gcc


override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \
  #-DGDB_INT_CFG

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)

