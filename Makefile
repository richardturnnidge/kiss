NAME=KISS
LDHAS_ARG_PROCESSING = 0
LDHAS_EXIT_HANDLER = 0
include $(shell agondev-config --makefile)

# AED's libraries: unpack aed-libs-1.6.2.tar.gz from
# https://github.com/avalonbits/aed/releases/tag/v1.6.2 next to this file.
# These lines must stay below the include: AgonDev's makefile sets CFLAGS and
# PROJECTLIBDIR itself, and would overwrite them if they came first.
AEDLIBS ?= aed-libs-1.6.2
CFLAGS += -I$(AEDLIBS)/include/core -I$(AEDLIBS)/include/ui
PROJECTLIBDIR := $(AEDLIBS)/lib
LIBS := -ledui -ledcore
