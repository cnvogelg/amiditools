CC=vc +aos68km

VBCC_TARGET_AMIGAOS ?= $(VBCC)/targets/m68k-amigaos/
VBCC_INC = $(VBCC_TARGET_AMIGAOS)/include
VBCC_LIB = $(VBCC_TARGET_AMIGAOS)/lib

NDK_DIR ?= $(HOME)/projects/amidev/ndk_3.9
NDK_INC = $(NDK_DIR)/include/include_h
NDK_LIB = $(NDK_DIR)/include/linker_libs
NDK_INC_ASM = $(NDK_DIR)/include/include_i

CAMD_INC = include
NET_INC ?= $(HOME)/projects/amidev/roadshow/netinclude

CFLAGS = -c99 -Os -+ -sc -I$(VBCC_INC) -I$(NDK_INC) -I$(CAMD_INC) -I$(NET_INC)
CFLAGS += -DKDEBUG=1
LDFLAGS = -L$(VBCC_LIB) -L$(NDK_LIB)
LDFLAGS_DRV = $(LDFLAGS) -nostdlib -ldebug

TOOLS=midi-info midi-echo midi-send midi-recv midi-udp midi-expunge
INSTALL_DIR=$(HOME)/amiga/shared
H?=@

all: $(TOOLS)

install: $(TOOLS)
	cp $(TOOLS) $(INSTALL_DIR)/

midi-info: midi-info.c
	@echo " CC  $@"
	$(H)$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lamiga

midi-expunge: midi-expunge.c
	@echo " CC  $@"
	$(H)$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lamiga

midi-echo: midi-echo.c midi-setup.c
	@echo " CC  $@"
	$(H)$(CC) $(CFLAGS) -o $@ $+ $(LDFLAGS) -lamiga

midi-send: midi-send.c midi-setup.c
	@echo " CC  $@"
	$(H)$(CC) $(CFLAGS) -o $@ $+ $(LDFLAGS) -lamiga

midi-recv: midi-recv.c midi-setup.c
	@echo " CC  $@"
	$(H)$(CC) $(CFLAGS) -o $@ $+ $(LDFLAGS) -lamiga

midi-udp: midi-udpdrv.c debug.c udp.c proto.c parser.c
	@echo " CC  $@"
	$(H)$(CC) $(CFLAGS) -o $@ $+ $(LDFLAGS_DRV) -lamiga

clean:
	rm -f $(TOOLS)
