###############################################################################
# "THE BEER-WARE LICENSE" (Revision 42):
# <msmith@FreeBSD.ORG> wrote this file. As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return
###############################################################################
#
# Makefile for building the blackbox data decoder.
#
# Invoke this with 'make help' to see the list of supported targets.
# 

###############################################################################
# Things that the user might override on the commandline
#

# Compile-time options
OPTIONS		?=
BLACKBOX_VERSION     ?= 

# Debugger optons, must be empty or GDB
DEBUG = GDB

###############################################################################
# Things that need to be maintained as the source changes
#

# Working directories
ROOT		 = $(dir $(lastword $(MAKEFILE_LIST)))
SRC_DIR		 = $(ROOT)/src
OBJECT_DIR	 = $(ROOT)/obj
BIN_DIR		 = $(ROOT)/obj

# Platform detection
IS_WINDOWS := $(if $(findstring MINGW,$(shell uname -s)),MINGW,$(if $(findstring MSYS,$(shell uname -s)),MSYS,$(if $(findstring CYGWIN,$(shell uname -s)),CYGWIN,)))

# Package config variables for better maintainability
CAIRO_CFLAGS    := $(shell pkg-config --cflags cairo)
CAIRO_LDFLAGS   := $(shell pkg-config --libs cairo)
FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2)
FREETYPE_LDFLAGS:= $(shell pkg-config --libs freetype2)

# Source files common to all targets
COMMON_SRC	 = parser.c tools.c platform.c stream.c decoders.c units.c blackbox_fielddefs.c

# Platform-specific sources
ifneq (,$(IS_WINDOWS))
	COMMON_SRC += lib/getopt_mb_uni/getopt.c
endif

DECODER_SRC	 = $(COMMON_SRC) blackbox_decode.c gpxwriter.c imu.c battery.c stats.c
RENDERER_SRC = $(COMMON_SRC) blackbox_render.c datapoints.c embeddedfont.c expo.c imu.c
ENCODER_TESTBED_SRC = $(COMMON_SRC) encoder_testbed.c encoder_testbed_io.c

# In some cases, %.s regarded as intermediate file, which is actually not.
# This will prevent accidental deletion of startup code.
.PRECIOUS: %.s

# Search path for baseflight sources
VPATH		:= $(SRC_DIR)

# Add Windows-specific source paths
ifneq (,$(IS_WINDOWS))
	VPATH := $(VPATH):$(ROOT)/lib/getopt_mb_uni
endif

###############################################################################
# Things that might need changing to use different tools
#

#
# Tool options.
#
INCLUDE_DIRS	 = $(SRC_DIR)

ifeq ($(DEBUG),GDB)
OPTIMIZE	 = -O0
LTO_FLAGS	 = $(OPTIMIZE)
else
OPTIMIZE	 = -O3
LTO_FLAGS	 = -flto $(OPTIMIZE)
endif

DEBUG_FLAGS	 = -g3 -ggdb

CFLAGS		= $(ARCH_FLAGS) \
		$(LTO_FLAGS) \
		$(addprefix -D,$(OPTIONS)) \
		$(addprefix -I,$(INCLUDE_DIRS)) \
		$(if $(strip $(BLACKBOX_VERSION)), -DBLACKBOX_VERSION=$(BLACKBOX_VERSION)) \
		$(DEBUG_FLAGS) \
		-std=gnu99 \
		-Wall -pedantic -Wextra -Wshadow

# Platform-specific configuration
ifneq (,$(IS_WINDOWS))
	# Windows/MINGW build using MSYS2 packages
	INCLUDE_DIRS += $(ROOT)/lib/getopt_mb_uni
	
	# Common Windows libraries for both executables
	WINDOWS_COMMON_LIBS = -static-libgcc -static-libstdc++ -lole32 -loleaut32 -luuid -lm
	
	# Graphics libraries only needed for renderer - try static first, fallback to dynamic
	WINDOWS_GRAPHICS_LIBS = $(shell pkg-config --libs --static cairo freetype2 2>/dev/null || pkg-config --libs cairo freetype2)
	WINDOWS_GRAPHICS_LIBS += -lgdi32 -lmsimg32
	
	# Renderer-specific CFLAGS (include graphics headers only for renderer)
	RENDERER_CFLAGS = $(CFLAGS) $(CAIRO_CFLAGS) $(FREETYPE_CFLAGS)
else
	# Unix-like systems (Linux, macOS)
	CFLAGS += -pthread
	
	# Renderer-specific CFLAGS (include graphics headers only for renderer)
	RENDERER_CFLAGS = $(CFLAGS) $(CAIRO_CFLAGS) $(FREETYPE_CFLAGS)
	ifeq ($(BUILD_STATIC), MACOSX)
		# For cairo built with ./configure --enable-quartz=no  --without-x --enable-pdf=no --enable-ps=no --enable-script=no --enable-xcb=no --enable-ft=yes --enable-fc=no --enable-xlib=no
		RENDERER_LDFLAGS = -Llib/macosx -lcairo -lpixman-1 -lpng16 -lz -lfreetype -lbz2 -lm -pthread
	else
		# Dynamic linking
		RENDERER_LDFLAGS = $(CAIRO_LDFLAGS) $(FREETYPE_LDFLAGS) -lm -pthread
	endif
	# Base LDFLAGS for decoder and encoder (no graphics libraries)
	LDFLAGS += -lm -pthread
endif


###############################################################################
# No user-serviceable parts below
###############################################################################

#
# Things we will build
#

DECODER_ELF	 = $(BIN_DIR)/blackbox_decode
RENDERER_ELF = $(BIN_DIR)/blackbox_render
ENCODER_TESTBED_ELF = $(BIN_DIR)/encoder_testbed

DECODER_OBJS	 = $(addsuffix .o,$(addprefix $(OBJECT_DIR)/,$(basename $(DECODER_SRC))))
RENDERER_OBJS	 = $(addsuffix .o,$(addprefix $(OBJECT_DIR)/,$(basename $(RENDERER_SRC))))
ENCODER_TESTBED_OBJS	 = $(addsuffix .o,$(addprefix $(OBJECT_DIR)/,$(basename $(ENCODER_TESTBED_SRC))))

TARGET_MAP   = $(OBJECT_DIR)/blackbox_decode.map

all : $(DECODER_ELF) $(RENDERER_ELF) $(ENCODER_TESTBED_ELF)

# Windows-specific linking rules to separate decoder from renderer dependencies
ifneq (,$(IS_WINDOWS))
$(DECODER_ELF):  $(DECODER_OBJS)
	@echo "=== LINKING DECODER (minimal dependencies) ==="
	@echo "Input objects: $^"
	@echo "Output: $@"
	@echo "Command: $(CC) -o $@ $^ $(WINDOWS_COMMON_LIBS)"
	$(CC) -o $@ $^ $(WINDOWS_COMMON_LIBS)
	@echo "✅ DECODER LINKING COMPLETE (size: $$(stat -c%s $@ 2>/dev/null || echo 'unknown') bytes)"
	@echo ""

$(RENDERER_ELF):  $(RENDERER_OBJS)
	@echo "=== LINKING RENDERER (with graphics libraries) ==="
	@echo "Input objects: $^"
	@echo "Output: $@"
	@echo "Command: $(CC) -o $@ $^ $(WINDOWS_COMMON_LIBS) $(WINDOWS_GRAPHICS_LIBS)"
	$(CC) -o $@ $^ $(WINDOWS_COMMON_LIBS) $(WINDOWS_GRAPHICS_LIBS)
	@echo "✅ RENDERER LINKING COMPLETE (size: $$(stat -c%s $@ 2>/dev/null || echo 'unknown') bytes)"
	@echo ""

$(ENCODER_TESTBED_ELF): $(ENCODER_TESTBED_OBJS)
	@echo "=== LINKING ENCODER TESTBED (minimal dependencies) ==="
	@echo "Input objects: $^"
	@echo "Output: $@"
	@echo "Command: $(CC) -o $@ $^ $(WINDOWS_COMMON_LIBS)"
	$(CC) -o $@ $^ $(WINDOWS_COMMON_LIBS)
	@echo "✅ ENCODER TESTBED LINKING COMPLETE (size: $$(stat -c%s $@ 2>/dev/null || echo 'unknown') bytes)"
	@echo ""
else
# Unix/Linux/macOS - use appropriate LDFLAGS for each target
$(DECODER_ELF):  $(DECODER_OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)

$(RENDERER_ELF):  $(RENDERER_OBJS)
	@$(CC) -o $@ $^ $(RENDERER_LDFLAGS)

$(ENCODER_TESTBED_ELF): $(ENCODER_TESTBED_OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)
endif

# Compile - separate rules for renderer files vs. other files
# Renderer-specific files that need graphics headers
$(OBJECT_DIR)/blackbox_render.o: blackbox_render.c
	@mkdir -p $(dir $@)
	@echo %% $(notdir $<) [with graphics headers]
	@$(CC) -c -o $@ $(RENDERER_CFLAGS) $<

$(OBJECT_DIR)/datapoints.o: datapoints.c
	@mkdir -p $(dir $@)
	@echo %% $(notdir $<) [with graphics headers]
	@$(CC) -c -o $@ $(RENDERER_CFLAGS) $<

$(OBJECT_DIR)/embeddedfont.o: embeddedfont.c
	@mkdir -p $(dir $@)
	@echo %% $(notdir $<) [with graphics headers]
	@$(CC) -c -o $@ $(RENDERER_CFLAGS) $<

$(OBJECT_DIR)/expo.o: expo.c
	@mkdir -p $(dir $@)
	@echo %% $(notdir $<) [with graphics headers]
	@$(CC) -c -o $@ $(RENDERER_CFLAGS) $<

# All other files use standard CFLAGS (no graphics headers)
$(OBJECT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo %% $(notdir $<)
	@$(CC) -c -o $@ $(CFLAGS) $<

clean:
	rm -f $(RENDERER_ELF) $(DECODER_ELF) $(ENCODER_TESTBED_ELF) $(ENCODER_TESTBED_OBJS) $(RENDERER_OBJS) $(DECODER_OBJS) $(TARGET_MAP)

help:
	@echo ""
	@echo "Makefile for the baseflight blackbox data decoder"
	@echo ""
	@echo "Usage:"
	@echo "        make [OPTIONS=\"<options>\"]"
	@echo ""
	@echo "Targets:"
	@echo "        all                      # Build all executables (default)"
	@echo "        clean                    # Clean build artifacts"
	@echo "        help                     # Show this help"
	@echo ""
	@echo "Windows debugging:"
	@echo "        make windows-pre-debug   # Check build environment before building"
	@echo "        make windows-debug       # Check executable dependencies after building"
	@echo "        make windows-collect-dlls # Collect required DLLs for distribution"
	@echo "        make windows-complete    # Build + debug + collect DLLs if needed"
	@echo ""
	@echo "Recommended Windows workflow:"
	@echo "        make windows-complete    # One-stop solution for Windows builds"
	@echo ""
	@echo "Debug targets:"
	@echo "        debug-platform           # Check platform detection"
	@echo ""

# Debug platform detection (always available)
debug-platform:
	@echo "=== Platform Detection Debug ==="
	@echo "uname -s: '$(shell uname -s)'"
	@echo "IS_WINDOWS: '$(IS_WINDOWS)'"
	@echo "Platform is Windows: $(if $(IS_WINDOWS),YES,NO)"
	@echo "Available Windows targets: $(if $(IS_WINDOWS),windows-debug windows-pre-debug windows-collect-dlls windows-complete,NONE - not Windows)"

# Enhanced Windows diagnostic target
ifneq (,$(IS_WINDOWS))
windows-debug: $(DECODER_ELF) $(RENDERER_ELF)
	@echo "=== WINDOWS BUILD DEBUG ==="
	@echo "Build completed successfully"
	@echo "Checking executable dependencies..."
	@ls -la $(DECODER_ELF) $(RENDERER_ELF) 2>/dev/null || echo "Executables not found"
	@echo "=== Dependency Check ==="
	@ldd $(DECODER_ELF) 2>/dev/null || echo "Decoder: No ldd available or static"
	@ldd $(RENDERER_ELF) 2>/dev/null || echo "Renderer: No ldd available or static"
	@echo "=== Debug Complete ==="

# Add a pre-build diagnostic target
windows-pre-debug:
	@echo "=== Pre-Build Windows Environment Check ==="
	@echo "System: $(shell uname -a)"
	@echo "uname -s output: '$(shell uname -s)'"
	@echo "IS_WINDOWS value: '$(IS_WINDOWS)'"
	@echo "Platform detected as Windows: $(if $(IS_WINDOWS),YES,NO)"
	@echo "Compiler version: $(shell $(CC) --version | head -1)"
	@echo "Make version: $(shell make --version | head -1)"
	@echo ""
	@echo "=== Package Availability ==="
	@echo "Cairo available: $(shell pkg-config --exists cairo && echo 'YES' || echo 'NO')"
	@echo "FreeType available: $(shell pkg-config --exists freetype2 && echo 'YES' || echo 'NO')"
	@echo "Cairo cflags: $(shell pkg-config --cflags cairo 2>/dev/null || echo 'FAILED')"
	@echo "FreeType cflags: $(shell pkg-config --cflags freetype2 2>/dev/null || echo 'FAILED')"
	@echo ""
endif

# Collect required DLLs for distribution if static linking fails
windows-collect-dlls: $(DECODER_ELF) $(RENDERER_ELF)
	@echo "=== Collecting Required DLLs for Distribution ==="
	@mkdir -p dist/windows
	@cp $(DECODER_ELF) $(RENDERER_ELF) $(ENCODER_TESTBED_ELF) dist/windows/ 2>/dev/null || true
	@echo "Executables copied to dist/windows/"
	@echo ""
	@echo "=== Analyzing DLL Dependencies ==="
	@echo "Renderer DLL requirements:"
	@ldd $(RENDERER_ELF) 2>/dev/null | grep -E "(cairo|freetype|pixman|png|harfbuzz|fontconfig)" | while read line; do \
		dll_path=$$(echo "$$line" | awk '{print $$3}'); \
		if [ -f "$$dll_path" ]; then \
			echo "  Copying: $$dll_path"; \
			cp "$$dll_path" dist/windows/ 2>/dev/null || echo "    ⚠️  Failed to copy $$dll_path"; \
		fi \
	done
	@echo ""
	@echo "=== Distribution Package Ready ==="
	@echo "Files in dist/windows:"
	@ls -la dist/windows/ 2>/dev/null || echo "No files collected"

# Combined target: build, debug, and collect DLLs if needed
windows-complete: all windows-debug
	@echo ""
	@echo "=== Checking if DLL collection is needed ==="
	@if ldd $(RENDERER_ELF) 2>/dev/null | grep -q "cairo\|freetype"; then \
		echo "⚠️  External DLLs detected - collecting for distribution"; \
		$(MAKE) windows-collect-dlls; \
	else \
		echo "✅ Static linking successful - no DLL collection needed"; \
	fi
