DIEHARD_TARGET   = libdiehard
DIEHARD_INCLUDE = -I./include -I./include/diehard

DIEHARDER_TARGET = libdieharder
DIEHARDER_INCLUDE = -I./include -I./include/dieharder

TARGET  = $(DIEHARD_TARGET)
INCLUDE = $(DIEHARD_INCLUDE)

UNIX_SRC  = Heap-Layers/wrappers/gnuwrapper.cpp source/libdieharder.cpp
MACOS_SRC = Heap-Layers/wrappers/macwrapper.cpp source/libdieharder.cpp

REPLICATED_SRC = replicated/replicated.cpp

# MACOS_COMPILER = g++
MACOS_COMPILER = llvm-g++

MACOS_COMMAND_STANDALONE := $(MACOS_COMPILER) -mpreferred-stack-boundary=4 -m128bit-long-double -msse2 -arch i386 -arch x86_64 -g -O3 -W -Wall -DDIEHARD_MULTITHREADED=1 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib  -D'CUSTOM_PREFIX(x)=xx\#\#x' $(MACOS_SRC) -o $(TARGET).dylib

MACOS_COMMAND_DEBUG_STANDALONE := $(MACOS_COMPILER) -msse2 -arch i386 -arch x86_64 -g -W -Wall -DDIEHARD_MULTITHREADED=1 $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib  -D'CUSTOM_PREFIX(x)=xx\#\#x' $(MACOS_SRC) -o $(TARGET).dylib

MACOS_COMMAND_REPLICATED := $(MACOS_COMPILER) -msse2 -pipe -O3 -finline-limit=65000 -fkeep-inline-functions -finline-functions -ffast-math -fomit-frame-pointer  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib $(MACOS_SRC) -o libdieharder_r.dylib

# LINUX_COMMAND_STANDALONE := g++ -m32 -malign-double -pipe -march=pentium4 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared -D'CUSTOM_PREFIX(x)=diehard\#\#x' $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_COMMAND_STANDALONE := g++ -m32 -finline-functions -malign-double -pipe -march=pentium4 -O3 -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared -D'CUSTOM_PREFIX(x)=diehard\#\#x' $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_COMMAND_REPLICATED := g++ -malign-double -pipe -march=pentium4 -O3 -fno-rtti -finline-functions  -ffast-math -fomit-frame-pointer -DNDEBUG  $(INCLUDE) -D_REENTRANT=1  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o libdieharder_r.so -ldl -lpthread

LINUX_64_COMMAND_STANDALONE := g++ -W -Wall -O3 -DNDEBUG -pipe -fPIC -m64 -march=nocona -ffast-math -g $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared  -D'CUSTOM_PREFIX(x)=diehard\#\#x' $(UNIX_SRC) -Bsymbolic -o $(TARGET).so -ldl -lpthread

LINUX_64_COMMAND_REPLICATED := g++ -O1 -DNDEBUG -pipe -fPIC -march=nocona -m64 -g $(INCLUDE) -D_REENTRANT=1  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -Bsymbolic -o libdieharder_r.so -ldl -lpthread

SOLARIS_SPARC32_COMMAND_STANDALONE := CC -xtarget=ultra2 -xildoff -xO5 -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o $(TARGET).so -lthread -ldl -lCrun

SOLARIS_SPARC32_COMMAND_REPLICATED := CC -xtarget=ultra2 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE)  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o libdieharder_r.so -lthread -ldl -lCrun

SOLARIS_SPARC64_COMMAND_STANDALONE := CC -xcode=pic13 -xtarget=ultra2 -m64 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o $(TARGET).so -lthread -ldl -lCrun

SOLARIS_SPARC64_COMMAND_REPLICATED := CC -xcode=pic13 -xtarget=ultra2 -m64 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG $(INCLUDE)  $(REPLICATED_SRC) -DDIEHARD_MULTITHREADED=1 -G -PIC $(UNIX_SRC) sparc-interchange.il -o libdieharder_r.so -lthread -ldl -lCrun

# After compiling:
# setenv LD_PRELOAD "$cwd/$(TARGET).so"
# and whenever g++ supports thread-level storage on Solaris...
# g++ -fPIC -O3 -finline-limit=65000 -mcpu=ultrasparc -DNDEBUG  $(INCLUDE) -D_REENTRANT=1 -DDIEHARD_MULTITHREADED=1 -shared $(UNIX_SRC) -o $(TARGET).so

DEPS = $(UNIX_SRC) $(MACOS_SRC) 

.PHONY: macos linux solaris

all:
	@echo "Type make <target>, where target is one of the following: "
	@echo "  linux-x86"
	@echo "  linux-x86-64"
	@echo "  solaris-sparc"
	@echo "  solaris-sparc-64"
	@echo "  macos"
	@echo "Add -replicated to make library for replicated execution."

linux-x86:
	$(LINUX_COMMAND_STANDALONE)

linux-x86-replicated:
	$(LINUX_COMMAND_REPLICATED)

linux-x86-64:
	$(LINUX_64_COMMAND_STANDALONE)

linux-x86-64-replicated:
	$(LINUX_64_COMMAND_REPLICATED)

solaris-sparc:
	$(SOLARIS_SPARC32_COMMAND_STANDALONE)

solaris-sparc-replicated:
	$(SOLARIS_SPARC32_COMMAND_REPLICATED)

solaris-sparc-64:
	$(SOLARIS_SPARC64_COMMAND_STANDALONE)

solaris-sparc-64-replicated:
	$(SOLARIS_SPARC64_COMMAND_REPLICATED)

macos:
	$(MACOS_COMMAND_STANDALONE)

macos-debug:
	$(MACOS_COMMAND_DEBUG_STANDALONE)

macos-replicated:
	$(MACOS_COMMAND_REPLICATED)


clean:
	rm -f $(TARGET).so libdieharder_r.so $(TARGET).dylib



