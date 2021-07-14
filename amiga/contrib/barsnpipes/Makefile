CAMD_NAMES=udp echo
CAMD_PORTS=0 1 2 3 4 5 6 7
VERSION ?= 1.0.0

VBCC_TARGET_AMIGAOS ?= $(VBCC)/targets/m68k-amigaos/
VBCC_INC = $(VBCC_TARGET_AMIGAOS)/include
VBCC_LIB = $(VBCC_TARGET_AMIGAOS)/lib

NDK_DIR ?= $(HOME)/projects/amidev/ndk_3.9
NDK_INC = $(NDK_DIR)/include/include_h
NDK_LIB = $(NDK_DIR)/include/linker_libs
NDK_INC_ASM = $(NDK_DIR)/include/include_i

CAMD_INC = ../../include
CAMD_NAME = udp
CAMD_PORT = 0

BNP_INC = include

QUOTE=\\\"

CC=vc +aos68km
CFLAGS = -c99 -Os -+ -sc -sd -I$(CAMD_INC) -I$(NDK_INC) -I$(BNP_INC)
CFLAGS += -DCAMD_NAME=$(QUOTE)$(CAMD_NAME)$(QUOTE)
CFLAGS += -DCAMD_PORT=$(QUOTE)$(CAMD_PORT)$(QUOTE)

LD=vc +aos68km
LDFLAGS = -sc -sd -nostdlib -lamiga -lvcs

TOOLSTART_SRC = toolstart.c
TOOL_IN_SRCS = camdin.c
TOOL_OUT_SRCS = camdout.c

BUILD_DIR = build
TOOLSTART_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TOOLSTART_SRC))
TOOL_IN_OBJS = $(patsubst %.c,$(BUILD_DIR)/%_$(CAMD_NAME)_$(CAMD_PORT).o,$(TOOL_IN_SRCS))
TOOL_OUT_OBJS = $(patsubst %.c,$(BUILD_DIR)/%_$(CAMD_NAME)_$(CAMD_PORT).o,$(TOOL_OUT_SRCS))
TOOL_IN = $(BUILD_DIR)/camd_$(CAMD_NAME)_in_$(CAMD_PORT).ptool
TOOL_OUT = $(BUILD_DIR)/camd_$(CAMD_NAME)_out_$(CAMD_PORT).ptool

DIST_DIR = dist
DIST_ARCHIVE = $(DIST_DIR)/amiditools_bnp-$(VERSION).lha

INSTALL_DIR=$(HOME)/amiga/shared

tools: init tool_in tool_out

all:
	@for name in $(CAMD_NAMES) ; do \
		for port in $(CAMD_PORTS) ; do \
			$(MAKE) CAMD_NAME=$$name CAMD_PORT=$$port ; \
		done ; \
	done

clean:
	rm -rf $(BUILD_DIR)

dist-clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)

dist: dist-all $(DIST_ARCHIVE)

$(DIST_ARCHIVE):
	@echo "  LHA  $@"
	@cd dist && lha a ../$@ *.ptool ../README.md

dist-all:
	@mkdir -p $(DIST_DIR)
	@for name in $(CAMD_NAMES) ; do \
		for port in $(CAMD_PORTS) ; do \
			$(MAKE) CAMD_NAME=$$name CAMD_PORT=$$port dist-tool ; \
		done ; \
	done

dist-tool: tools
	@echo "  DIST  $(CAMD_NAME) $(CAMD_PORT)"
	@cp $(TOOL_IN) $(DIST_DIR)/
	@cp $(TOOL_OUT) $(DIST_DIR)/

install: all
	cp $(BUILD_DIR)/*.ptool $(INSTALL_DIR)/

init: $(BUILD_DIR)

tool_in: $(TOOL_IN)

tool_out: $(TOOL_OUT)

$(TOOL_IN): $(TOOLSTART_OBJ) $(TOOL_IN_OBJS)
	@echo "  LINK $(<F)"
	$(HIDE)$(LD) $^ -o $@ $(LDFLAGS)

$(TOOL_OUT): $(TOOLSTART_OBJ) $(TOOL_OUT_OBJS)
	@echo "  LINK $(<F)"
	$(HIDE)$(LD) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o : %.c
	@echo "  CC   $(<F)"
	$(HIDE)$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%_$(CAMD_NAME)_$(CAMD_PORT).o : %.c
	@echo "  CC   $(<F)"
	$(HIDE)$(CC) -c $(CFLAGS) $< -o $@
