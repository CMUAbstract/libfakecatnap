LIB = libfakecatnap

OBJECTS += \
  catnap.o

DEPS +=

override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)

