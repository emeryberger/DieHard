# Commands to compile DieHard / DieHarder for various targets.
# Run make (with no arguments) to see the complete target list.

all:
	@echo
	@echo To build DieHard / DieHarder, specify the desired build target:
	@echo -----------------------------------------------------------------
	@echo freebsd
	@echo linux-gcc-x86
	@echo linux-gcc-x86-64
	@echo macos
	@echo solaris-sunw-sparc
	@echo solaris-sunw-x86
	@echo solaris-sunw-x86-64
	@echo solaris-gcc-sparc
	@echo generic-gcc
	@echo windows
	@echo -----------------------------------------------------------------
	@echo Add -replicated to build for replicated execution;
	@echo not supported on all targets.
	@echo "Specify TARGET=libdiehard to build DieHard (default)."
	@echo Specify TARGET=libdieharder to build DieHarder.
	@echo


.PHONY: macos freebsd linux-gcc-x86 linux-gcc-x86-debug solaris-sunw-sparc solaris-sunw-x86 solaris-gcc-sparc generic-gcc linux-gcc-x86-64 windows clean

DIEHARD_TARGET    := libdiehard
DIEHARDER_TARGET  := libdieharder

TARGET ?= libdiehard

# WIN_INCLUDES = /I. /Iinclude /Iinclude/layers /Iinclude/util /Iinclude/math /Iinclude/static /Iinclude/rng /Iinclude/hoard /Iinclude/superblocks /IHeap-Layers

INCLUDES = -I. -Iinclude -Iinclude/layers -Iinclude/util -Iinclude/math -Iinclude/static -Iinclude/rng -Iinclude/hoard -Iinclude/superblocks -IHeap-Layers

# -IHeap-Layers/arch-specific -IHeap-Layers/wrappers -IHeap-Layers/utility -IHeap-Layers/locks -IHeap-Layers/threads -IHeap-Layers/heaps/buildingblock -IHeap-Layers/heaps/threads -IHeap-Layers/heaps/utility -IHeap-Layers/heaps/general -IHeap-Layers/heaps/combining -IHeap-Layers/heaps/objectrep -IHeap-Layers/heaps/top -IHeap-Layers/heaps/debug -IHeap-Layers/heaps/special


ifeq ($(TARGET),libdieharder)
INCLUDE := $(INCLUDES) -DDIEHARD_DIEHARDER=1
else
INCLUDE := $(INCLUDES) -DDIEHARD_DIEHARDER=0
endif

UNIX_SRC  = Heap-Layers/wrappers/gnuwrapper.cpp source/libdieharder.cpp
# UNIX_SRC  = Heap-Layers/wrappers/gnuwrapper-hooks.cpp source/libdieharder.cpp
MACOS_SRC = Heap-Layers/wrappers/macwrapper.cpp source/libdieharder.cpp

DEPS = Heap-Layers $(MACOS_SRC) $(UNIX_SRC)

Heap-Layers:
	git clone https://github.com/emeryberger/Heap-Layers

REPLICATED_SRC = replicated/replicated.cpp

MACOS_COMPILER = g++
# MACOS_COMPILER = llvm-g++

WIN_FLAGS = /Iinclude /Zi /Ox /Oy /DNDEBUG /MD /DDIEHARD_REPLICATED=0 

WINDOWS_COMMAND_STANDALONE := cl /LD $(WIN_INCLUDES) $(WIN_FLAGS) winhard.cpp /link /base:0x63000000 kernel32.lib user32.lib msvcrt.lib advapi32.lib /subsystem:console /dll /incremental:no /entry:HardDllMain

MACOS_COMMAND_STANDALONE := $(MACOS_COMPILER) --std=c++17 -arch x86_64 -g -O3 -W -Wall -DDIEHARD_MULTITHREADED=1 -DNDEBUG -DDIEHARD_DIEFAST=1 $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o $(TARGET).dylib

MACOS_COMMAND_DEBUG_STANDALONE := $(MACOS_COMPILER) --std=c++14 -arch x86_64 -g -W -Wall -DDIEHARD_MULTITHREADED=1 $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o $(TARGET).dylib

MACOS_COMMAND_REPLICATED := $(MACOS_COMPILER) -pipe -O3 -finline-limit=65000 -fkeep-inline-functions -finline-functions -ffast-math -fomit-frame-pointer  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o libdieharder_r.dylib

LINUX_COMMAND_STANDALONE := $(CXX) -std=c++14 -m32 -finline-functions -malign-double -pipe -march=pentium4 -O3 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_COMMAND_REPLICATED := $(CXX) -std=c++14 -malign-double -pipe -march=pentium4 -O3 -fno-rtti -finline-functions  -ffast-math -fomit-frame-pointer -DNDEBUG  $(INCLUDE) -D_REENTRANT=1  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o libdieharder_r.so -ldl -lpthread

FLAGS = -pedantic -Wall -Wpointer-arith -Woverloaded-virtual -Werror=return-type -Wtype-limits -Wempty-body -Wno-ctor-dtor-privacy -Wno-overlength-strings -Wno-invalid-offsetof -Wno-variadic-macros -Wcast-align -Wno-long-long -pthread -pipe  -DNDEBUG

LINUX_64_COMMAND_STANDALONE_DEBUG := $(CXX) -std=c++14 -W -Wall -O0 -pipe -fPIC -m64 -ffast-math -g $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared  $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_64_COMMAND_STANDALONE := clang++ $(FLAGS) -std=c++14 -W -Wall -O3 -DNDEBUG -pipe -fPIC -m64 -ffast-math -g $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1  -DDIEHARD_DIEFAST=1 -ferror-limit=1 -shared  $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_64_COMMAND_REPLICATED := $(CXX) -std=c++14 -O1 -DNDEBUG -pipe -fPIC -m64 -g $(INCLUDE) -D_REENTRANT=1  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o libdieharder_r.so -ldl -lpthread

SOLARIS_SPARC32_COMMAND_STANDALONE := CC -xtarget=ultra2 -xildoff -xO5 -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o $(TARGET).so -lthread -ldl -lCrun

SOLARIS_SPARC32_COMMAND_REPLICATED := CC -xtarget=ultra2 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE)  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o libdieharder_r.so -lthread -ldl -lCrun

SOLARIS_SPARC64_COMMAND_STANDALONE := CC -xcode=pic13 -xtarget=ultra2 -m64 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o $(TARGET).so -lthread -ldl -lCrun

SOLARIS_SPARC64_COMMAND_REPLICATED := CC -xcode=pic13 -xtarget=ultra2 -m64 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE)  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o libdieharder_r.so -lthread -ldl -lCrun


linux-gcc-x86: $(DEPS)
	$(LINUX_COMMAND_STANDALONE)

linux-gcc-x86-replicated: $(DEPS)
	$(LINUX_COMMAND_REPLICATED)

linux-gcc-x86-64: $(DEPS)
	$(LINUX_64_COMMAND_STANDALONE)

linux-gcc-x86-64-debug: $(DEPS)
	$(LINUX_64_COMMAND_STANDALONE_DEBUG)

linux-gcc-x86-64-replicated: $(DEPS)
	$(LINUX_64_COMMAND_REPLICATED)

solaris-sparc: $(DEPS)
	$(SOLARIS_SPARC32_COMMAND_STANDALONE)

solaris-sparc-replicated: $(DEPS)
	$(SOLARIS_SPARC32_COMMAND_REPLICATED)

solaris-sparc-64: $(DEPS)
	$(SOLARIS_SPARC64_COMMAND_STANDALONE)

solaris-sparc-64-replicated: $(DEPS)
	$(SOLARIS_SPARC64_COMMAND_REPLICATED)

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



