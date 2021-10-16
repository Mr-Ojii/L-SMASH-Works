#----------------------------------------------------------------------------------------------
#  Makefile for AviUtl plugins
#----------------------------------------------------------------------------------------------

include config.mak

vpath %.c $(SRCDIR)
vpath %.h $(SRCDIR)
vpath %.rc $(SRCDIR)

OBJ_INPUT = $(SRC_INPUT:%.c=%.o) lwinput.res
OBJ_MUXER = $(SRC_MUXER:%.c=%.o) lwmuxer.res
OBJ_DUMPER = $(SRC_DUMPER:%.c=%.o)
OBJ_COLOR = $(SRC_COLOR:%.c=%.o)
OBJ_BRIDGE = $(SRC_BRIDGE:%.c=%.o)

SRC_ALL = $(SRC_INPUT) $(SRC_MUXER) $(SRC_DUMPER) $(SRC_COLOR) $(SRC_BRIDGE)

RCFLAGS = -J rc -O coff

ifneq ($(STRIP),)
LDFLAGS += -Wl,-s
endif

.PHONY: all input muxer dumper color bridge inputexe clean distclean dep

all: input muxer dumper color bridge inputexe

input: lwinput.aui

muxer: lwmuxer.auf

dumper: lwdumper.auf

color: lwcolor.auc

bridge: lwbridge.aui

inputexe: lwinput.exe

lwinput.aui: $(OBJ_INPUT)
	$(LD) $(LDFLAGS) $(AU_LDFLAGS) -o $@ $^ $(LIBS)

lwmuxer.auf: $(OBJ_MUXER)
	$(LD) $(LDFLAGS) $(AU_LDFLAGS) -o $@ $^ $(LIBS)

lwdumper.auf: $(OBJ_DUMPER)
	$(LD) $(LDFLAGS) $(AU_LDFLAGS) -o $@ $^ $(LIBS)

lwcolor.auc: $(OBJ_COLOR)
	$(LD) $(LDFLAGS) $(AU_LDFLAGS) -o $@ $^ $(LIBS)

lwbridge.aui: $(OBJ_BRIDGE)
	$(LD) $(LDFLAGS) $(AU_LDFLAGS) -o $@ $^ $(LIBS)

lwinput.exe: $(OBJ_INPUT)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c .depend
	$(CC) $(CFLAGS) -c $< -o $@

%.res: %.rc resource.h
	$(RC) $(RCFLAGS) $< -o $@

clean:
	$(RM) *.auf *.aui *.auc *.exe *.o *.res .depend

distclean: clean
	$(RM) config.*

dep: .depend

ifneq ($(wildcard .depend),)
include .depend
endif

.depend: config.mak
	@$(RM) .depend
	@$(foreach SRC, $(SRC_ALL:%=$(SRCDIR)/%), $(CC) $(SRC) $(CFLAGS) -msse4.1 -g0 -MT $(SRC:$(SRCDIR)/%.c=%.o) -MM >> .depend;)

config.mak:
	configure
