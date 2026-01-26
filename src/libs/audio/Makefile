# Makefile for audio library with intelligent dependency detection
CC = gcc
CFLAGS = -Wall -Wextra -O3 -fPIC -std=c99
LDFLAGS = -shared -Wl,--export-dynamic
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

# Function to check if a library exists
define check_lib
$(shell pkg-config --exists $(1) 2>/dev/null && echo "yes" || echo "no")
endef

# Function to check if a static library file exists
define check_static_lib
$(shell find /usr/lib /usr/local/lib /lib -name "lib$(1).a" 2>/dev/null | head -1)
endef

# MSYS2/MinGW-w64 with complete static linking
ifneq (,$(findstring MINGW,$(UNAME_S)))
    # Use g++ for linking when we have C++ dependencies (like ASIO)
    CC = x86_64-w64-mingw32-gcc
    CXX = x86_64-w64-mingw32-g++
    LIBNAME = libaudio.dll
    CFLAGS = -Wall -Wextra -O3 -fPIC -std=c99 -I/mingw64/include -m64 -DPA_USE_ASIO=1

    # Critical: Add static linking flags and correct pthread linking with linker groups
    LDFLAGS = -shared -Wl,--export-all-symbols -static-libgcc -static-libstdc++ -static -L/mingw64/lib -m64 -Wl,-Bstatic -lpthread -Wl,-Bdynamic

    # Core libraries with full dependency chain (order is critical!)
    CORE_LIBS =
    # Check for local Windows-specific libportaudio first, then system location
    ifneq (,$(wildcard ./libportaudio-win.a))
        CORE_LIBS += ./libportaudio-win.a
    else ifneq (,$(wildcard /mingw64/lib/libportaudio.a))
        CORE_LIBS += /mingw64/lib/libportaudio.a
    else
        CORE_LIBS += -lportaudio
    endif

    # libsndfile and ALL its dependencies in dependency order
    ifneq (,$(wildcard /mingw64/lib/libsndfile.a))
        CORE_LIBS += /mingw64/lib/libsndfile.a

        # Codec libraries (dependencies of libsndfile)
        ifneq (,$(wildcard /mingw64/lib/libmpg123.a))
            CORE_LIBS += /mingw64/lib/libmpg123.a
        endif
        ifneq (,$(wildcard /mingw64/lib/libmp3lame.a))
            CORE_LIBS += /mingw64/lib/libmp3lame.a
        endif
        ifneq (,$(wildcard /mingw64/lib/libopus.a))
            CORE_LIBS += /mingw64/lib/libopus.a
        endif
        ifneq (,$(wildcard /mingw64/lib/libFLAC.a))
            CORE_LIBS += /mingw64/lib/libFLAC.a
        endif

        # Handle vorbis/ogg dependency chain properly
        ifneq (,$(wildcard /mingw64/lib/libvorbisenc.a))
            ifneq (,$(wildcard /mingw64/lib/libogg.a))
                CORE_LIBS += /mingw64/lib/libvorbisenc.a /mingw64/lib/libvorbisfile.a /mingw64/lib/libvorbis.a /mingw64/lib/libogg.a
            else
                $(warning Found vorbis libraries but missing libogg.a - skipping vorbis support)
            endif
        else ifneq (,$(wildcard /mingw64/lib/libogg.a))
            CORE_LIBS += /mingw64/lib/libogg.a
        endif
        ifneq (,$(wildcard /mingw64/lib/libspeex.a))
            CORE_LIBS += /mingw64/lib/libspeex.a
        endif
    else
        CORE_LIBS += -lsndfile
    endif

    ifneq (,$(wildcard /mingw64/lib/libspeexdsp.a))
        CORE_LIBS += /mingw64/lib/libspeexdsp.a
        CFLAGS += -DHAVE_SPEEXDSP
    else
        CORE_LIBS += -lspeexdsp
    endif

    # Windows system libraries (these provide the missing symbols)
    SYSTEM_LIBS = -lwinmm -lole32 -luuid -lksuser -lsetupapi -lws2_32 -ladvapi32 -luser32 -lgdi32 -lkernel32

    # Additional Windows libraries for codec dependencies
    SYSTEM_LIBS += -lshlwapi -lshell32 -lcomdlg32 -lcomctl32 -lrpcrt4

    # Math and C runtime - remove redundant pthread linking here since it's in LDFLAGS
    SYSTEM_LIBS += -lm -lmsvcrt -lstdc++

    LIBS = $(CORE_LIBS) $(SYSTEM_LIBS)
    OBJ_EXT = .o

else ifeq ($(UNAME_S),Darwin)
    # macOS
    LIBNAME = libaudio.dylib
    CFLAGS += -I/usr/local/include -I/opt/homebrew/include
    LDFLAGS = -shared -undefined dynamic_lookup -L/usr/local/lib -L/opt/homebrew/lib

    # Core libraries - check for local macOS-specific portaudio first
    CORE_LIBS =
    ifneq (,$(wildcard ./libportaudio-macos.a))
        CORE_LIBS += ./libportaudio-macos.a
    else
        CORE_LIBS += -lportaudio
    endif

    CORE_LIBS += -lsndfile
    CORE_LIBS += -lspeexdsp

    # macOS frameworks
    LIBS = $(CORE_LIBS) -framework CoreAudio -framework AudioToolbox -framework AudioUnit
    LIBS += -framework CoreFoundation -framework CoreServices
    OBJ_EXT = .o

else ifeq ($(UNAME_S),Linux)
    # Linux with intelligent library detection
    LIBNAME = libaudio.so

    # Check for pkg-config availability
    PKG_CONFIG := $(shell command -v pkg-config 2> /dev/null)

    # Core audio libraries (required) - check for local Linux-specific portaudio first
    CORE_LIBS =
    ifneq (,$(wildcard ./libportaudio-linux.a))
        CORE_LIBS += ./libportaudio-linux.a
    else
        CORE_LIBS += -lportaudio
    endif

    CORE_LIBS += -lsndfile

    ifeq ($(call check_lib,speexdsp),yes)
        CORE_LIBS += -lspeexdsp
        CFLAGS += -DHAVE_SPEEXDSP
    endif

    # Audio backend libraries (optional)
    BACKEND_LIBS =

    # Check for ALSA
    ifeq ($(call check_lib,alsa),yes)
        BACKEND_LIBS += -lasound
        CFLAGS += -DHAVE_ALSA
    endif

    # Check for PulseAudio
    ifeq ($(call check_lib,libpulse),yes)
        BACKEND_LIBS += -lpulse
        CFLAGS += -DHAVE_PULSE
    endif

    ifeq ($(call check_lib,libpulse-simple),yes)
        BACKEND_LIBS += -lpulse-simple
        CFLAGS += -DHAVE_PULSE_SIMPLE
    endif

    # Check for JACK (optional)
    ifeq ($(call check_lib,jack),yes)
        BACKEND_LIBS += -ljack
        CFLAGS += -DHAVE_JACK
    endif

    # Codec libraries (optional)
    CODEC_LIBS =
    ifeq ($(call check_lib,flac),yes)
        CODEC_LIBS += -lFLAC
        CFLAGS += -DHAVE_FLAC
    endif

    ifeq ($(call check_lib,vorbisenc),yes)
        CODEC_LIBS += -lvorbisenc -lvorbisfile -lvorbis
        CFLAGS += -DHAVE_VORBIS
    endif

    ifeq ($(call check_lib,ogg),yes)
        CODEC_LIBS += -logg
        CFLAGS += -DHAVE_OGG
    endif

    # System libraries
    SYSTEM_LIBS = -lpthread -lm -ldl

    # Combine all libraries
    LIBS = $(CORE_LIBS) $(BACKEND_LIBS) $(CODEC_LIBS) $(SYSTEM_LIBS)
    OBJ_EXT = .o

else
    # Generic Unix fallback - minimal dependencies
    LIBNAME = libaudio.so
    LIBS = -lportaudio -lsndfile -lspeexdsp -lpthread -lm
    OBJ_EXT = .o
endif

SOURCES = $(wildcard *.c)

# Object files
ifneq (,$(findstring Windows,$(UNAME_S)))
    OBJECTS = $(SOURCES:.c=.obj)
else
    OBJECTS = $(SOURCES:.c=.o)
endif

.PHONY: all clean install debug check-deps list-libs minimal

all: check-deps $(LIBNAME)

# Build targets
ifneq (,$(findstring Windows,$(UNAME_S)))
$(LIBNAME): $(OBJECTS)
	# Check if we need C++ linking (if libportaudio-win.a contains C++ code)
	@if [ -f "./libportaudio-win.a" ] && nm ./libportaudio-win.a 2>/dev/null | grep -q "operator\|__gxx_personality"; then \
		echo "Detected C++ code in libportaudio-win.a - using C++ linker"; \
		$(CXX) $(LDFLAGS) -o $@ $^ -Wl,--start-group $(LIBS) -Wl,--end-group; \
	else \
		echo "Using C linker"; \
		$(CC) $(LDFLAGS) -o $@ $^ -Wl,--start-group $(LIBS) -Wl,--end-group; \
	fi

%.obj: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-del /Q *.obj $(LIBNAME) 2>nul || rm -f $(OBJECTS) $(LIBNAME)

else
# Unix/Linux/macOS build rules
$(LIBNAME): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBNAME)

install: $(LIBNAME)
	sudo cp $(LIBNAME) /usr/local/lib/
	sudo ldconfig 2>/dev/null || true
endif

# Check dependencies and show what will be used
check-deps:
	@echo "=== Dependency Check ==="
	@echo "Target system: $(UNAME_S)"
	@echo "Library name: $(LIBNAME)"
	@echo "Compiler: $(CC)"
ifneq (,$(findstring MINGW,$(UNAME_S)))
	@echo "=== MinGW Libraries ==="
	@echo "Checking for Windows-specific libportaudio:"
	@ls -la ./libportaudio-win.a 2>/dev/null && echo "✓ Found local libportaudio-win.a" || echo "✗ No local libportaudio-win.a"
	@echo "Libraries to link: $(LIBS)"
else ifeq ($(UNAME_S),Linux)
	@echo "=== Linux Package Check ==="
	@echo "Checking for Linux-specific libportaudio:"
	@ls -la ./libportaudio-linux.a 2>/dev/null && echo "✓ Found local libportaudio-linux.a" || echo "✗ No local libportaudio-linux.a"
ifdef PKG_CONFIG
	@echo "pkg-config available: yes"
	@echo -n "PortAudio: "; pkg-config --exists portaudio-2.0 && echo "✓" || echo "✗"
	@echo -n "libsndfile: "; pkg-config --exists sndfile && echo "✓" || echo "✗"
	@echo -n "speexdsp: "; pkg-config --exists speexdsp && echo "✓" || echo "✗"
	@echo -n "ALSA: "; pkg-config --exists alsa && echo "✓" || echo "✗"
	@echo -n "PulseAudio: "; pkg-config --exists libpulse && echo "✓" || echo "✗"
	@echo -n "JACK: "; pkg-config --exists jack && echo "✓" || echo "✗"
else
	@echo "pkg-config not available"
endif
	@echo "Libraries to link: $(LIBS)"
else ifeq ($(UNAME_S),Darwin)
	@echo "=== macOS Package Check ==="
	@echo "Checking for macOS-specific libportaudio:"
	@ls -la ./libportaudio-macos.a 2>/dev/null && echo "✓ Found local libportaudio-macos.a" || echo "✗ No local libportaudio-macos.a"
	@echo "Libraries to link: $(LIBS)"
else
	@echo "Libraries to link: $(LIBS)"
endif

# Show detected libraries
list-libs:
	@echo "Core libraries: $(CORE_LIBS)"
	@echo "Backend libraries: $(BACKEND_LIBS)"
	@echo "Codec libraries: $(CODEC_LIBS)"
	@echo "System libraries: $(SYSTEM_LIBS)"
	@echo "All libraries: $(LIBS)"

# Build with only essential libraries (fallback)
minimal: override LIBS = -lportaudio -lsndfile -lspeexdsp -lpthread -lm
minimal: $(LIBNAME)
	@echo "Built minimal version with basic dependencies only"

# Development target
debug: CFLAGS += -g -DDEBUG
debug: $(LIBNAME)

# Verification target
verify: $(LIBNAME)
	@echo "=== Library Information ==="
	@file $(LIBNAME)
ifneq (,$(findstring MINGW,$(UNAME_S)))
	@echo "=== Dependencies ==="
	@objdump -p $(LIBNAME) | grep "DLL Name:" | head -10 || echo "No dependencies found"
else ifeq ($(UNAME_S),Linux)
	@echo "=== Dependencies ==="
	@ldd $(LIBNAME) 2>/dev/null | head -10 || echo "Static build or no dependencies"
endif

help:
	@echo "Available targets:"
	@echo "  all       - Build the audio library (default)"
	@echo "  minimal   - Build with minimal dependencies"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install library (Unix/Linux/macOS only)"
	@echo "  debug     - Build with debug symbols"
	@echo "  check-deps- Check for required dependencies"
	@echo "  list-libs - Show detected library configuration"
	@echo "  verify    - Verify built library and show dependencies"
	@echo "  help      - Show this help message"
