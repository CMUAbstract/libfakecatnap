LIB = libfakecatnap

OBJECTS += \
  catnap.o \
  events.o \
  fifos.o \
  timers.o \
  checkpoint.o \
  hw.o \
  comp.o \
  scheduler.o

DEPS += libcapybara:gcc

# 2.0V, (2.0*100) and our alg takes care of it
LIBFCN_EVENT_THRESHOLD = 200
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

