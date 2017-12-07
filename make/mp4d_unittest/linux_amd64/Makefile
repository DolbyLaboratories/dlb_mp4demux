#************************************************************************************************************
# * Copyright (c) 2017, Dolby Laboratories Inc.
# * All rights reserved.
#
# * Redistribution and use in source and binary forms, with or without modification, are permitted
# * provided that the following conditions are met:
#
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
# *    and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
# *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
# *    promote products derived from this software without specific prior written permission.
#
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
# * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
# * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# * OF THE POSSIBILITY OF SUCH DAMAGE.
#************************************************************************************************************

#-*-makefile-*-

# colorized output can be disabled (e.g. for log files) by calling make with COLOR=0
ifneq ($(COLOR),0)
COL_OUTPUT=\033[33m
COL_END=\033[0m
endif

# to dump the complete compiler/linker/archiver commandlines, call make with VERBOSE=1
ifeq ($(VERBOSE),1)
QUIET=
else
AT=@
QUIET=--quiet
endif

ECHO=echo
PRINTF=printf
MKDIR_P=mkdir -p

ifeq ($(OS),Windows_NT)
ifneq ($(TERM),cygwin)
RM=del
endif
endif

all: mp4d_unittest_release mp4d_unittest_debug

.PHONY: help force
force: ;
help:
	$(AT)$(ECHO) "This makefile has the following targets:"
	$(AT)$(ECHO) "	all"
	$(AT)$(ECHO) "	mp4d_unittest_release"
	$(AT)$(ECHO) "	mp4d_unittest_debug"
	$(AT)$(ECHO) "	clean"


BASE=../../../

# Compile files for mp4d_unittest_release
CC_mp4d_unittest_release=$(CC)
CFLAGS_OUTPUT_FILE_mp4d_unittest_release=-o 
CFLAGS_mp4d_unittest_release=\
  -Wextra \
  -O3 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)include \
  -I$(BASE)src \
  -I$(BASE)test

CCDEP_mp4d_unittest_release=$(CC)
CCDEPFLAGS_mp4d_unittest_release=\
  -MM \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)include \
  -I$(BASE)src \
  -I$(BASE)test \
  -MT

CCDEPFLAGS_OUTPUT_FILE_mp4d_unittest_release=-o 
OBJS_mp4d_unittest_release=\
  obj/mp4d_unittest_release/mp4d_unittest.o

DEPS_mp4d_unittest_release=\
  obj/mp4d_unittest_release/mp4d_unittest.d


obj/mp4d_unittest_release:
	$(AT)$(MKDIR_P) obj/mp4d_unittest_release



include $(wildcard obj/mp4d_unittest_release/mp4d_unittest.d)

	
obj/mp4d_unittest_release/mp4d_unittest.o: $(BASE)test/mp4d_unittest.c | obj/mp4d_unittest_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_unittest_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_unittest_release) $(CCDEPFLAGS_mp4d_unittest_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_unittest_release)obj/mp4d_unittest_release/mp4d_unittest.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_unittest_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_unittest_release) $(CFLAGS_mp4d_unittest_release) $(CFLAGS_OUTPUT_FILE_mp4d_unittest_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





# Compile files for mp4d_unittest_debug
CC_mp4d_unittest_debug=$(CC)
CFLAGS_OUTPUT_FILE_mp4d_unittest_debug=-o 
CFLAGS_mp4d_unittest_debug=\
  -Wextra \
  -g \
  -ggdb3 \
  -O0 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -I$(BASE). \
  -I$(BASE)include \
  -I$(BASE)src \
  -I$(BASE)test

CCDEP_mp4d_unittest_debug=$(CC)
CCDEPFLAGS_mp4d_unittest_debug=\
  -MM \
  -I$(BASE). \
  -I$(BASE)include \
  -I$(BASE)src \
  -I$(BASE)test \
  -MT

CCDEPFLAGS_OUTPUT_FILE_mp4d_unittest_debug=-o 
OBJS_mp4d_unittest_debug=\
  obj/mp4d_unittest_debug/mp4d_unittest.o

DEPS_mp4d_unittest_debug=\
  obj/mp4d_unittest_debug/mp4d_unittest.d


obj/mp4d_unittest_debug:
	$(AT)$(MKDIR_P) obj/mp4d_unittest_debug



include $(wildcard obj/mp4d_unittest_debug/mp4d_unittest.d)

	
obj/mp4d_unittest_debug/mp4d_unittest.o: $(BASE)test/mp4d_unittest.c | obj/mp4d_unittest_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_unittest_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_unittest_debug) $(CCDEPFLAGS_mp4d_unittest_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_unittest_debug)obj/mp4d_unittest_debug/mp4d_unittest.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_unittest_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_unittest_debug) $(CFLAGS_mp4d_unittest_debug) $(CFLAGS_OUTPUT_FILE_mp4d_unittest_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





LD_mp4d_unittest_release=gcc
LDFLAGS_mp4d_unittest_release=-O2
LDLIBS_mp4d_unittest_release=
LDFLAGS_OUTPUT_FILE_mp4d_unittest_release=-o 

# Link mp4d_unittest_release
mp4d_unittest_release: $(OBJS_mp4d_unittest_release) $(BASE)make/mp4d/linux_amd64/mp4d_release.a
	$(AT)$(ECHO) "[LD:gcc] $^"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(LD_mp4d_unittest_release) $(LDFLAGS_mp4d_unittest_release) $(LDFLAGS_OUTPUT_FILE_mp4d_unittest_release)$@ $^ $(LDLIBS_mp4d_unittest_release)
	$(AT)$(PRINTF) "$(COL_END)"

$(BASE)make/mp4d/linux_amd64/mp4d_release.a: force
	$(AT)make $(QUIET) --no-print-directory -C $(BASE)make/mp4d/linux_amd64 mp4d_release.a



LD_mp4d_unittest_debug=gcc
LDFLAGS_mp4d_unittest_debug=-rdynamic
LDLIBS_mp4d_unittest_debug=
LDFLAGS_OUTPUT_FILE_mp4d_unittest_debug=-o 

# Link mp4d_unittest_debug
mp4d_unittest_debug: $(OBJS_mp4d_unittest_debug) $(BASE)make/mp4d/linux_amd64/mp4d_debug.a
	$(AT)$(ECHO) "[LD:gcc] $^"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(LD_mp4d_unittest_debug) $(LDFLAGS_mp4d_unittest_debug) $(LDFLAGS_OUTPUT_FILE_mp4d_unittest_debug)$@ $^ $(LDLIBS_mp4d_unittest_debug)
	$(AT)$(PRINTF) "$(COL_END)"

$(BASE)make/mp4d/linux_amd64/mp4d_debug.a: force
	$(AT)make $(QUIET) --no-print-directory -C $(BASE)make/mp4d/linux_amd64 mp4d_debug.a



clean:
	$(RM) $(OBJS_mp4d_unittest_release)
	$(RM) $(DEPS_mp4d_unittest_release)
	$(RM) $(OBJS_mp4d_unittest_debug)
	$(RM) $(DEPS_mp4d_unittest_debug)
	$(RM) mp4d_unittest_release
	make -C $(BASE)make/mp4d/linux_amd64 clean
	$(RM) mp4d_unittest_debug
