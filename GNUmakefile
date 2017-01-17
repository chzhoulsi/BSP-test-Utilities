# -*- makefile -*-
######################################################################
#
# GNUmakefile
#
# See the file named README in this directory for more information.
#
# Note: Assumes that make is really GNU make.
#
######################################################################

##########
# Macros #
##########

SHELL = /bin/sh

ifdef CROSS_COMPILE
ifndef LINUX
$(error "LINUX must be defined")
endif
ifndef SYSROOT
$(error "SYSROOT must be defined")
endif
endif

CC := $(CROSS_COMPILE)gcc
#CFLAGS := -g -O2 -Werror -I.
CFLAGS := -g -Werror -I.
ifdef CROSS_COMPILE
CFLAGS += --sysroot=$(SYSROOT) -I $(SYSROOT)/usr/include
endif

LD = $(CC)
LDFLAGS := $(CFLAGS) -L$(SYSROOT)/lib -L$(SYSROOT)/usr/lib

STRIP = $(CROSS_COMPILE)strip

BUILD = $(CROSS_COMPILE)build
BUILD_DIRECTORY = $(BUILD)

MAKE_BUILD_DIRECTORY = \
@if [ ! -d $(BUILD_DIRECTORY) ] ; then \
	mkdir $(BUILD_DIRECTORY) ; \
fi

$(BUILD_DIRECTORY)/%.o: %.c
	$(MAKE_BUILD_DIRECTORY)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIRECTORY)/%.d: %.c
	$(MAKE_BUILD_DIRECTORY)
	@$(SHELL) -ec '$(CC) -M $(CFLAGS) $< | sed '\''s/\($*\)\.o[ :]*/$(BUILD_DIRECTORY)\/\1.o $(BUILD_DIRECTORY)\/$(notdir $@) : /g'\'' > $@'

SOURCES = util.c image.c 
OBJECTS = $(addprefix $(BUILD_DIRECTORY)/,$(patsubst %.c,%.o,$(SOURCES)))
DEPENDENCIES = $(addprefix $(BUILD_DIRECTORY)/,$(patsubst %.c,%.d,$(SOURCES)))

###########
# Targets #
###########

.PHONY: all configure config build clean distclean install

all: clean configure build 

configure:

config: configure

build: $(BUILD_DIRECTORY)/image 

clean:
	@rm -rf *.tar.gz *~ $(BUILD_DIRECTORY)

distclean: clean

install:
	@echo "Just copy $(BUILD_DIRECTORY)/image to its final location."

archive: README.h GNUmakefile $(SOURCES) util.h
	rm -f rbupdate.tar rbupdate.tar.gz
	tar cf rbupdate.tar $^
	gzip rbupdate.tar

$(BUILD_DIRECTORY)/image: \
	$(BUILD_DIRECTORY)/util.o $(BUILD_DIRECTORY)/image.o
	$(LD) $(LDFLAGS) -o $@ $^
	cp $@ $@.debug
	$(STRIP) $@

$(BUILD_DIRECTORY)/splparms: \
	$(BUILD_DIRECTORY)/util.o $(BUILD_DIRECTORY)/splparms.o
	$(LD) $(LDFLAGS) -o $@ $^
	cp $@ $@.debug
	$(STRIP) $@

################
# Dependencies #
################

-include $(DEPENDENCIES)
