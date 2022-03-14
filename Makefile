LIB = libfakecatnap

OBJECTS += \
  catnap.o \
  events.o \
  fifos.o \
  timers.o \
  hw.o

DEPS += libcapybara:gcc

override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)

