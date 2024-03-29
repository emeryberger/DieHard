# Commands to compile DieHard / DieHarder for various targets.
# Run make (with no arguments) to see the complete target list.

all:
	@echo
	@echo To build DieHard / DieHarder, specify the desired build target:
	@echo -----------------------------------------------------------------
	@echo freebsd
	@echo linux-gcc-32
	@echo linux-gcc-64
	@echo macos
	@echo generic-gcc
	@echo windows
	@echo -----------------------------------------------------------------
	@echo Add -replicated to build for replicated execution;
	@echo not supported on all targets.
	@echo "Specify TARGET=libdiehard to build DieHard (default)."
	@echo Specify TARGET=libdieharder to build DieHarder.
	@echo


.PHONY: macos freebsd linux-gcc-32 linux-gcc-32-debug generic-gcc linux-gcc-64 linux-gcc-64-debug windows clean

DIEHARD_TARGET    := libdiehard
DIEHARDER_TARGET  := libdieharder

TARGET ?= libdiehard

INCLUDES = -I. -Iinclude -Iinclude/layers -Iinclude/util -Iinclude/math -Iinclude/static -Iinclude/rng -Iinclude/hoard -Iinclude/superblocks -IHeap-Layers

ifeq ($(TARGET),libdieharder)
INCLUDE := $(INCLUDES) -DDIEHARD_DIEHARDER=1
else
INCLUDE := $(INCLUDES) -DDIEHARD_DIEHARDER=0
endif

UNIX_SRC  = Heap-Layers/wrappers/gnuwrapper.cpp source/libdieharder.cpp
MACOS_SRC = Heap-Layers/wrappers/macwrapper.cpp source/libdieharder.cpp

$(MACOS_SRC): Heap-Layers
$(UNIX_SRC): Heap-Layers

DEPS = Heap-Layers $(MACOS_SRC) $(UNIX_SRC)

Heap-Layers:
	git clone https://github.com/emeryberger/Heap-Layers

REPLICATED_SRC = replicated/replicated.cpp

MACOS_COMPILER = $(CXX)
# MACOS_COMPILER = llvm-g++

WIN_FLAGS = /Iinclude /Zi /Ox /Oy /DNDEBUG /MD /DDIEHARD_REPLICATED=0 

WINDOWS_COMMAND_STANDALONE := cl /LD $(WIN_INCLUDES) $(WIN_FLAGS) winhard.cpp /link /base:0x63000000 kernel32.lib user32.lib msvcrt.lib advapi32.lib /subsystem:console /dll /incremental:no /entry:HardDllMain

MACOS_COMMAND_STANDALONE := $(MACOS_COMPILER) --std=c++17 -arch x86_64 -arch arm64 -g -O3 -W -Wall -DDIEHARD_MULTITHREADED=1 -DNDEBUG -DDIEHARD_DIEFAST=0 $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o $(TARGET).dylib

MACOS_COMMAND_DEBUG_STANDALONE := $(MACOS_COMPILER) --std=c++14 -arch x86_64 -arch arm64 -g -W -Wall -DDIEHARD_MULTITHREADED=1 $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o $(TARGET).dylib

MACOS_COMMAND_REPLICATED := $(MACOS_COMPILER) -arch x86_64 -arch arm64 -pipe -O3 -finline-limit=65000 -fkeep-inline-functions -finline-functions -ffast-math -fomit-frame-pointer  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o libdieharder_r.dylib

LINUX_COMMAND_STANDALONE := $(CXX) -std=c++14 -m32 -finline-functions -malign-double -pipe -march=pentium4 -O3 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_COMMAND_REPLICATED := $(CXX) -std=c++14 -malign-double -pipe -march=pentium4 -O3 -fno-rtti -finline-functions  -ffast-math -fomit-frame-pointer -DNDEBUG  $(INCLUDE) -D_REENTRANT=1  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o libdieharder_r.so -ldl -lpthread

FLAGS = -pedantic -Wall -Wpointer-arith -Woverloaded-virtual -Werror=return-type -Wtype-limits -Wempty-body -Wno-ctor-dtor-privacy -Wno-overlength-strings -Wno-invalid-offsetof -Wno-variadic-macros -Wcast-align -Wno-long-long -pthread -pipe  -DNDEBUG

LINUX_64_COMMAND_STANDALONE_DEBUG := $(CXX) -std=c++14 -W -Wall -O0 -pipe -fPIC  -ffast-math -g $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared  $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_64_COMMAND_STANDALONE := clang++ $(FLAGS) -std=c++14 -W -Wall -O3 -DNDEBUG -pipe -fPIC -ffast-math -g $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1  -DDIEHARD_DIEFAST=0 -ferror-limit=1 -shared  $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_64_COMMAND_REPLICATED := $(CXX) -std=c++14 -O1 -DNDEBUG -pipe -fPIC -m64 -g $(INCLUDE) -D_REENTRANT=1  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o libdieharder_r.so -ldl -lpthread


linux-gcc: $(DEPS)
	$(LINUX_COMMAND_STANDALONE)

linux-gcc-replicated: $(DEPS)
	$(LINUX_COMMAND_REPLICATED)

linux-gcc-64: $(DEPS)
	$(LINUX_64_COMMAND_STANDALONE)

linux-gcc-64-debug: $(DEPS)
	$(LINUX_64_COMMAND_STANDALONE_DEBUG)

linux-gcc-64-replicated: $(DEPS)
	$(LINUX_64_COMMAND_REPLICATED)

windows: $(DEPS)
	$(WINDOWS_COMMAND_STANDALONE)

macos: $(DEPS)
	$(MACOS_COMMAND_STANDALONE)

macos-debug: $(DEPS)
	$(MACOS_COMMAND_DEBUG_STANDALONE)

macos-replicated: $(DEPS)
	$(MACOS_COMMAND_REPLICATED)

clean:
	rm -f $(TARGET).so libdieharder_r.so $(TARGET).dylib



