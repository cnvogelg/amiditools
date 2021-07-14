# build Amiga tools with vbcc/vasm/vlink

FLAVOR?=debug
VERSION?=1.0.0

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

CFLAGS_debug += -DKDEBUG=1 -g
CFLAGS = -c99 -Os -+ -sc -sd -I$(VBCC_INC) -I$(NDK_INC) -I$(CAMD_INC) -I$(NET_INC)
CFLAGS += $(CFLAGS_$(FLAVOR))
LDFLAGS = -Os -+ -sc -sd -L$(VBCC_LIB) -L$(NDK_LIB) -lamiga
LDFLAGS_DRV = $(LDFLAGS) -nostdlib -ldebug
LDFLAGS_debug = -g
LDFLAGS += $(LDFLAGS_$(FLAVOR))

BUILD_DIR=build
BIN_DIR=$(BUILD_DIR)/$(FLAVOR)
OBJ_DIR=$(BUILD_DIR)/$(FLAVOR)/obj
DIST_BASE_DIR=dist
DIST_DIR=$(DIST_BASE_DIR)/$(FLAVOR)
DIST_ARCHIVE=$(DIST_BASE_DIR)/amiditools-$(VERSION).lha
INSTALL_DIR=$(HOME)/amiga/shared

HIDE?=@
TOOLS :=
VPATH=src src/drv

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
MIDI_SEND_SRCS=midi-send.c midi-tools.c midi-setup.c cmd.c
$(eval $(call build-app,midi-send,$(MIDI_SEND_SRCS)))

# midi-recv
MIDI_RECV_SRCS=midi-recv.c midi-tools.c midi-setup.c cmd.c
$(eval $(call build-app,midi-recv,$(MIDI_RECV_SRCS)))

# midi-perf
MIDI_PERF_SRCS=midi-perf.c midi-tools.c midi-setup.c cmd.c debug.c
$(eval $(call build-app,midi-perf,$(MIDI_PERF_SRCS)))

# common midi driver sources
MIDI_DRV_SRCS=midi-drv.c debug.c midi-parser.c

# midi-drv-echo
MIDI_DRV_ECHO_SRCS=$(MIDI_DRV_SRCS) midi-drv-echo.c
$(eval $(call build-drv,midi-drv-echo,$(MIDI_DRV_ECHO_SRCS)))

# midi-drv-udp
MIDI_DRV_UDP_SRCS=$(MIDI_DRV_SRCS) midi-drv-udp.c udp.c proto.c
$(eval $(call build-drv,midi-drv-udp,$(MIDI_DRV_UDP_SRCS)))

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
	cp -f midi-test $(INSTALL_DIR)/

install-release:
	$(MAKE) FLAVOR=release install

clean:
	rm -rf $(BIN_DIR) $(DIST_DIR)

clean-all:
	rm -rf $(BUILD_DIR) $(DIST_BASE_DIR)

clean-install:
	rm -rf $(INSTALL_FILES) $(INSTALL_DIR)/midi-test

dist-flavor: init $(DIST_DIR) $(DIST_FILES)

.PHONY: dist

dist: $(DIST_ARCHIVE)
dist-clean: clean-all

$(DIST_ARCHIVE):
	@$(MAKE) dist-flavor
	@$(MAKE) dist-flavor FLAVOR=release
	@echo "  LHA  $@"
	@cd dist && lha a ../$@ debug release ../../README.md

.PHONY: dist
