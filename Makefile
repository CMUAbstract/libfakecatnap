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

# 2.0V for ADC with 1M:1M divider
LIBFCN_EVENT_THRESHOLD = 1600
LIBFCN_COMP_PIN_PORT = 3
LIBFCN_COMP_PIN_PIN = 1

override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \
  -DLFCN_STARTER_THRESH=$(LIBFCN_EVENT_THRESHOLD) \
  -DLFCN_VCAP_COMP_PIN_PORT=$(LIBFCN_COMP_PIN_PORT) \
  -DLFCN_VCAP_COMP_PIN_PIN=$(LIBFCN_COMP_PIN_PIN) \
  #-DGDB_INT_CFG

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)

