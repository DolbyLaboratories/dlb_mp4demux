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

all: mp4d_release.a mp4d_debug.a

.PHONY: help force
force: ;
help:
	$(AT)$(ECHO) "This makefile has the following targets:"
	$(AT)$(ECHO) "	all"
	$(AT)$(ECHO) "	mp4d_release.a"
	$(AT)$(ECHO) "	mp4d_debug.a"
	$(AT)$(ECHO) "	clean"


BASE=../../../

# Compile files for mp4d_release
CC_mp4d_release=$(CC)
CFLAGS_OUTPUT_FILE_mp4d_release=-o 
CFLAGS_mp4d_release=\
  -Wextra \
  -O3 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -DEZXML_NOMMAP=X \
  -DNDEBUG=1 \
  -D_FILE_OFFSET_BITS=64 \
  -I$(BASE). \
  -I$(BASE)include

CCDEP_mp4d_release=$(CC)
CCDEPFLAGS_mp4d_release=\
  -MM \
  -DEZXML_NOMMAP=X \
  -DNDEBUG=1 \
  -D_FILE_OFFSET_BITS=64 \
  -I$(BASE). \
  -I$(BASE)include\
  -MT

CCDEPFLAGS_OUTPUT_FILE_mp4d_release=-o 
OBJS_mp4d_release=\
  obj/mp4d_release/mp4d_nav.o \
  obj/mp4d_release/mp4d_trackreader.o \
  obj/mp4d_release/mp4d_demux.o \
  obj/mp4d_release/mp4d_box_read.o \
  obj/mp4d_release/mp4d_buffer.o \
  obj/mp4d_release/player.o \
  obj/mp4d_release/es_sink.o \
  obj/mp4d_release/stream.o \
  obj/mp4d_release/file_movie.o \
  obj/mp4d_release/md_sink.o \
  obj/mp4d_release/fragment_stream.o \
  obj/mp4d_release/file_stream.o \
  obj/mp4d_release/util.o

DEPS_mp4d_release=\
  obj/mp4d_release/mp4d_nav.d \
  obj/mp4d_release/mp4d_trackreader.d \
  obj/mp4d_release/mp4d_demux.d \
  obj/mp4d_release/mp4d_box_read.d \
  obj/mp4d_release/mp4d_buffer.d \
  obj/mp4d_release/player.d \
  obj/mp4d_release/es_sink.d \
  obj/mp4d_release/stream.d \
  obj/mp4d_release/file_movie.d \
  obj/mp4d_release/md_sink.d \
  obj/mp4d_release/fragment_stream.d \
  obj/mp4d_release/file_stream.d \
  obj/mp4d_release/util.d


obj/mp4d_release:
	$(AT)$(MKDIR_P) obj/mp4d_release



include $(wildcard obj/mp4d_release/mp4d_nav.d)

	
obj/mp4d_release/mp4d_nav.o: $(BASE)src/mp4d_nav.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/mp4d_nav.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/mp4d_trackreader.d)

	
obj/mp4d_release/mp4d_trackreader.o: $(BASE)src/mp4d_trackreader.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/mp4d_trackreader.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/mp4d_demux.d)

	
obj/mp4d_release/mp4d_demux.o: $(BASE)src/mp4d_demux.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/mp4d_demux.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/mp4d_box_read.d)

	
obj/mp4d_release/mp4d_box_read.o: $(BASE)src/mp4d_box_read.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/mp4d_box_read.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/mp4d_buffer.d)

	
obj/mp4d_release/mp4d_buffer.o: $(BASE)src/mp4d_buffer.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/mp4d_buffer.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/player.d)

	
obj/mp4d_release/player.o: $(BASE)src/player.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/player.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/es_sink.d)

	
obj/mp4d_release/es_sink.o: $(BASE)src/es_sink.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/es_sink.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/stream.d)

	
obj/mp4d_release/stream.o: $(BASE)src/stream.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/file_movie.d)

	
obj/mp4d_release/file_movie.o: $(BASE)src/file_movie.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/file_movie.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/md_sink.d)

	
obj/mp4d_release/md_sink.o: $(BASE)src/md_sink.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/md_sink.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/fragment_stream.d)

	
obj/mp4d_release/fragment_stream.o: $(BASE)src/fragment_stream.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/fragment_stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_release/file_stream.d)

	
obj/mp4d_release/file_stream.o: $(BASE)src/file_stream.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/file_stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"

include $(wildcard obj/mp4d_release/util.d)

	
obj/mp4d_release/util.o: $(BASE)src/util.c | obj/mp4d_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_release) $(CCDEPFLAGS_mp4d_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_release)obj/mp4d_release/util.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_release) $(CFLAGS_mp4d_release) $(CFLAGS_OUTPUT_FILE_mp4d_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"



# Compile files for mp4d_debug
CC_mp4d_debug=$(CC)
CFLAGS_OUTPUT_FILE_mp4d_debug=-o 
CFLAGS_mp4d_debug=\
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
  -DEZXML_NOMMAP=X \
  -D_FILE_OFFSET_BITS=64 \
  -I$(BASE). \
  -I$(BASE)include

CCDEP_mp4d_debug=$(CC)
CCDEPFLAGS_mp4d_debug=\
  -MM \
  -DEZXML_NOMMAP=X \
  -D_FILE_OFFSET_BITS=64 \
  -I$(BASE). \
  -I$(BASE)include\
  -MT

CCDEPFLAGS_OUTPUT_FILE_mp4d_debug=-o 
OBJS_mp4d_debug=\
  obj/mp4d_debug/mp4d_nav.o \
  obj/mp4d_debug/mp4d_trackreader.o \
  obj/mp4d_debug/mp4d_demux.o \
  obj/mp4d_debug/mp4d_box_read.o \
  obj/mp4d_debug/mp4d_buffer.o \
  obj/mp4d_debug/player.o \
  obj/mp4d_debug/es_sink.o \
  obj/mp4d_debug/stream.o \
  obj/mp4d_debug/file_movie.o \
  obj/mp4d_debug/md_sink.o \
  obj/mp4d_debug/fragment_stream.o \
  obj/mp4d_debug/file_stream.o \
  obj/mp4d_debug/util.o

DEPS_mp4d_debug=\
  obj/mp4d_debug/mp4d_nav.d \
  obj/mp4d_debug/mp4d_trackreader.d \
  obj/mp4d_debug/mp4d_demux.d \
  obj/mp4d_debug/mp4d_box_read.d \
  obj/mp4d_debug/mp4d_buffer.d\
  obj/mp4d_debug/player.d \
  obj/mp4d_debug/es_sink.d \
  obj/mp4d_debug/stream.d \
  obj/mp4d_debug/file_movie.d \
  obj/mp4d_debug/md_sink.d \
  obj/mp4d_debug/fragment_stream.d \
  obj/mp4d_debug/file_stream.d \
  obj/mp4d_debug/util.d


obj/mp4d_debug:
	$(AT)$(MKDIR_P) obj/mp4d_debug



include $(wildcard obj/mp4d_debug/mp4d_nav.d)

	
obj/mp4d_debug/mp4d_nav.o: $(BASE)src/mp4d_nav.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/mp4d_nav.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/mp4d_trackreader.d)

	
obj/mp4d_debug/mp4d_trackreader.o: $(BASE)src/mp4d_trackreader.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/mp4d_trackreader.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/mp4d_demux.d)

	
obj/mp4d_debug/mp4d_demux.o: $(BASE)src/mp4d_demux.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/mp4d_demux.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/mp4d_box_read.d)

	
obj/mp4d_debug/mp4d_box_read.o: $(BASE)src/mp4d_box_read.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/mp4d_box_read.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/mp4d_buffer.d)

	
obj/mp4d_debug/mp4d_buffer.o: $(BASE)src/mp4d_buffer.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/mp4d_buffer.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/player.d)

	
obj/mp4d_debug/player.o: $(BASE)src/player.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/player.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/es_sink.d)

	
obj/mp4d_debug/es_sink.o: $(BASE)src/es_sink.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/es_sink.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/stream.d)

	
obj/mp4d_debug/stream.o: $(BASE)src/stream.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/file_movie.d)

	
obj/mp4d_debug/file_movie.o: $(BASE)src/file_movie.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/file_movie.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/md_sink.d)

	
obj/mp4d_debug/md_sink.o: $(BASE)src/md_sink.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/md_sink.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/fragment_stream.d)

	
obj/mp4d_debug/fragment_stream.o: $(BASE)src/fragment_stream.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/fragment_stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/file_stream.d)

	
obj/mp4d_debug/file_stream.o: $(BASE)src/file_stream.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/file_stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4d_debug/util.d)

	
obj/mp4d_debug/util.o: $(BASE)src/util.c | obj/mp4d_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4d_debug) $(CCDEPFLAGS_mp4d_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4d_debug)obj/mp4d_debug/util.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4d_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4d_debug) $(CFLAGS_mp4d_debug) $(CFLAGS_OUTPUT_FILE_mp4d_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"




AR_mp4d_release=ar
ARFLAGS_mp4d_release=-rc
ARFLAGS_OUTPUT_FILE_mp4d_release= 

mp4d_release.a: $(OBJS_mp4d_release)
	$(AT)$(ECHO) "[AR:$(AR_mp4d_release)] $@"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(AR_mp4d_release) $(ARFLAGS_mp4d_release) $(ARFLAGS_OUTPUT_FILE_mp4d_release)$@ $^
	$(AT)$(PRINTF) "$(COL_END)"

AR_mp4d_debug=ar
ARFLAGS_mp4d_debug=-rc
ARFLAGS_OUTPUT_FILE_mp4d_debug= 

mp4d_debug.a: $(OBJS_mp4d_debug)
	$(AT)$(ECHO) "[AR:$(AR_mp4d_debug)] $@"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(AR_mp4d_debug) $(ARFLAGS_mp4d_debug) $(ARFLAGS_OUTPUT_FILE_mp4d_debug)$@ $^
	$(AT)$(PRINTF) "$(COL_END)"

clean:
	$(RM) $(OBJS_mp4d_release)
	$(RM) $(DEPS_mp4d_release)
	$(RM) $(OBJS_mp4d_debug)
	$(RM) $(DEPS_mp4d_debug)
	$(RM) mp4d_release.a
	$(RM) mp4d_debug.a
