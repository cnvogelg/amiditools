# build Amiga tools with vbcc/vasm/vlink

FLAVOR?=debug

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

CFLAGS_debug += -DKDEBUG=1
CFLAGS = -c99 -Os -+ -sc -I$(VBCC_INC) -I$(NDK_INC) -I$(CAMD_INC) -I$(NET_INC)
CFLAGS += $(CFLAGS_$(FLAVOR))
LDFLAGS = -Os -+ -sc -L$(VBCC_LIB) -L$(NDK_LIB) -lamiga
LDFLAGS_DRV = $(LDFLAGS) -nostdlib -ldebug

BUILD_DIR=build
BIN_DIR=$(BUILD_DIR)/$(FLAVOR)
OBJ_DIR=$(BUILD_DIR)/$(FLAVOR)/obj
DIST_BASE_DIR=dist
DIST_DIR=$(DIST_BASE_DIR)/$(FLAVOR)
INSTALL_DIR=$(HOME)/amiga/shared

HIDE?=@
TOOLS :=
VPATH=src

# ----- rules -----
all: init build

# compile
$(OBJ_DIR)/%.o : %.c
	@echo "  CC   $(<F)"
	$(HIDE)$(CC) -c $(CFLAGS) $< -o $@

$(DIST_DIR)/%: $(BIN_DIR)/%
	@echo "  DIST $(@F)"
	$(HIDE)cp $< $@

$(INSTALL_DIR)/%: $(BIN_DIR)/%
	@echo "  INST $(@F)"
	$(HIDE)cp $< $@

# build app
# $1 = app name
# $2 = src files
define build-app
TOOLS += $(BIN_DIR)/$1
$1: $(BIN_DIR)/$1
$(BIN_DIR)/$1: $(patsubst %.c,$(OBJ_DIR)/%.o,$2)
	@echo "  APP  $$(@F)"
	$(HIDE)$(CC) -o $$@ $$+ $(LDFLAGS)
endef

# build drv
# $1 = drv name
# $2 = src files
define build-drv
TOOLS += $(BIN_DIR)/$1
$1: $(BIN_DIR)/$1
$(BIN_DIR)/$1: $(patsubst %.c,$(OBJ_DIR)/%.o,$2)
	@echo "  DRV  $$(@F)"
	$(HIDE)$(CC) -o $$@ $$+ $(LDFLAGS_DRV)
endef

# midi-info
MIDI_INFO_SRCS=midi-info.c
$(eval $(call build-app,midi-info,$(MIDI_INFO_SRCS)))

# midi-expunge
MIDI_EXPUNGE_SRCS=midi-expunge.c
$(eval $(call build-app,midi-expunge,$(MIDI_EXPUNGE_SRCS)))

# midi-echo
MIDI_ECHO_SRCS=midi-echo.c midi-setup.c
$(eval $(call build-app,midi-echo,$(MIDI_ECHO_SRCS)))

# midi-send
MIDI_SEND_SRCS=midi-send.c midi-setup.c
$(eval $(call build-app,midi-send,$(MIDI_SEND_SRCS)))

# midi-recv
MIDI_RECV_SRCS=midi-recv.c midi-setup.c
$(eval $(call build-app,midi-recv,$(MIDI_RECV_SRCS)))

# midi-drv-udp
MIDI_UDPDRV_SRCS=midi-udpdrv.c debug.c udp.c proto.c parser.c
$(eval $(call build-drv,midi-udpdrv,$(MIDI_UDPDRV_SRCS)))

init: $(BIN_DIR) $(OBJ_DIR)
	@echo "  FLAVOR=$(FLAVOR)"

build: $(TOOLS)

release: 
	$(MAKE) FLAVOR=release

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(DIST_DIR):
	@mkdir -p $(DIST_DIR)

DIST_FILES=$(patsubst $(BIN_DIR)/%,$(DIST_DIR)/%,$(TOOLS))

INSTALL_FILES=$(patsubst $(BIN_DIR)/%,$(INSTALL_DIR)/%,$(TOOLS))

install: init $(INSTALL_FILES)

clean:
	rm -rf $(BIN_DIR) $(DIST_DIR)

clean-all:
	rm -rf $(BUILD_DIR) $(DIST_BASE_DIR)

dist-flavor: init $(DIST_DIR) $(DIST_FILES)

dist:
	@$(MAKE) dist-flavor
	@$(MAKE) dist-flavor FLAVOR=release

.PHONY: dist
