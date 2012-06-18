FLAGS = \
    -Wextra  -Wall -Waggregate-return -Wcast-align \
    -Wcast-qual  -Wchar-subscripts  -Wcomment -Wconversion \
    -Wdisabled-optimization \
    -Wfloat-equal  -Wformat  -Wformat=2 \
    -Wformat-nonliteral -Wformat-security  \
    -Wformat-y2k \
    -Wimplicit  -Wimport  -Winit-self  -Winline \
    -Winvalid-pch   \
    -Wunsafe-loop-optimizations  -Wlong-long -Wmissing-braces \
    -Wmissing-field-initializers -Wmissing-format-attribute   \
    -Wmissing-include-dirs -Wmissing-noreturn \
    -Wpacked  -Wparentheses  -Wpointer-arith \
    -Wredundant-decls -Wreturn-type \
    -Wsequence-point  -Wshadow -Wsign-compare  -Wstack-protector \
    -Wstrict-aliasing -Wstrict-aliasing=2 -Wswitch  -Wswitch-default \
    -Wswitch-enum -Wtrigraphs  -Wuninitialized \
    -Wunknown-pragmas -Wunused \
    -Wunused-function  -Wunused-label  -Wunused-parameter \
    -Wunused-value  -Wunused-variable  -Wvariadic-macros \
    -Wvolatile-register-var  -Wwrite-strings

# MACOS_COMMAND_STANDALONE := g++ -arch i386 -arch x86_64 -g -W -Wall -Wextra -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -DNDEBUG  -I. -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib  -D'CUSTOM_PREFIX(x)=xx\#\#x' macwrapper.cpp libdiehard.cpp -o libdiehard.dylib

MACOS_COMMAND_STANDALONE := g++ -arch i386 -arch x86_64 -g -O3 -W -Wall -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -DNDEBUG  -I. -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib  -D'CUSTOM_PREFIX(x)=xx\#\#x' macwrapper.cpp libdiehard.cpp -o libdiehard.dylib

# MACOS_COMMAND_STANDALONE := g++ -O3 -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -DNDEBUG  -I. -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib libdiehard.cpp -o libdiehard.dylib

MACOS_COMMAND_REPLICATED := g++ -pipe -O3 -finline-limit=65000 -fkeep-inline-functions -finline-functions -ffast-math -fomit-frame-pointer -DDIEHARD_REPLICATED=1 -DDIEHARD_MULTITHREADED=1 -DNDEBUG  -I. -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib libdiehard.cpp -o libdiehard_r.dylib

# LINUX_COMMAND_STANDALONE := g++ -m32 -malign-double -pipe -march=pentium4 -DNDEBUG  -I. -D_REENTRANT=1 -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -shared -D'CUSTOM_PREFIX(x)=diehard\#\#x' libdiehard.cpp gnuwrapper.cpp -Bsymbolic -o libdiehard.so -ldl -lpthread

LINUX_COMMAND_STANDALONE := g++ -m32 -finline-functions -malign-double -pipe -march=pentium4 -O3 -DNDEBUG  -I. -D_REENTRANT=1 -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -shared -D'CUSTOM_PREFIX(x)=xx\#\#x' libdiehard.cpp gnuwrapper.cpp -Bsymbolic -o libdiehard.so -ldl -lpthread

LINUX_COMMAND_REPLICATED := g++ -malign-double -pipe -march=pentium4 -O3 -fno-rtti -finline-functions  -ffast-math -fomit-frame-pointer -DNDEBUG  -I. -D_REENTRANT=1 -DDIEHARD_REPLICATED=1 -DDIEHARD_MULTITHREADED=1 -shared libdiehard.cpp -Bsymbolic -o libdiehard_r.so -ldl -lpthread

LINUX_64_COMMAND_STANDALONE := g++ -W -Wall -O3 -DNDEBUG -pipe -fPIC -m64 -march=nocona -ffast-math -g -I. -D_REENTRANT=1 -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -shared  -D'CUSTOM_PREFIX(x)=xx\#\#x' gnuwrapper.cpp libdiehard.cpp -Bsymbolic -o libdiehard.so -ldl -lpthread

LINUX_64_COMMAND_REPLICATED := g++ -O1 -DNDEBUG -pipe -fPIC -march=nocona -m64 -g -I. -D_REENTRANT=1 -DDIEHARD_REPLICATED=1 -DDIEHARD_MULTITHREADED=1 -shared libdiehard.cpp -Bsymbolic -o libdiehard_r.so -ldl -lpthread

SOLARIS_SPARC32_COMMAND_STANDALONE := CC -xtarget=ultra2 -xildoff -xO5 -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG -I. -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -G -PIC libdiehard.cpp sparc-interchange.il -o libdiehard.so -lthread -ldl -lCrun

SOLARIS_SPARC32_COMMAND_REPLICATED := CC -xtarget=ultra2 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG -I. -DDIEHARD_REPLICATED=1 -DDIEHARD_MULTITHREADED=1 -G -PIC libdiehard.cpp sparc-interchange.il -o libdiehard_r.so -lthread -ldl -lCrun

SOLARIS_SPARC64_COMMAND_STANDALONE := CC -xcode=pic13 -xtarget=ultra2 -m64 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG -I. -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -G -PIC libdiehard.cpp sparc-interchange.il -o libdiehard.so -lthread -ldl -lCrun

SOLARIS_SPARC64_COMMAND_REPLICATED := CC -xcode=pic13 -xtarget=ultra2 -m64 -dalign -xbuiltin=%all -xO5 -xildoff -xthreadvar=dynamic -L/usr/lib/lwp -R/usr/lib/lwp -DNDEBUG -I. -DDIEHARD_REPLICATED=1 -DDIEHARD_MULTITHREADED=1 -G -PIC libdiehard.cpp sparc-interchange.il -o libdiehard_r.so -lthread -ldl -lCrun

# After compiling:
# setenv LD_PRELOAD "$cwd/libdiehard.so"
# and whenever g++ supports thread-level storage on Solaris...
# g++ -fPIC -O3 -finline-limit=65000 -mcpu=ultrasparc -DNDEBUG  -I. -D_REENTRANT=1 -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -shared libdiehard.cpp -o libdiehard.so

DEPS = libdiehard.cpp bitmap.h \
	bumpalloc.h heapshield.cpp largeheap.h lockheap.h log2.h \
	marsaglia.h mmapalloc.h mmapwrapper.h platformspecific.h \
	randomheap.h diehardheap.h randomminiheap.h \
	randomnumbergenerator.h realrandomvalue.h sassert.h \
	sparc-interchange.il staticif.h staticlog.h  \
	usewinhard.cpp version.h wrapper.cpp

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

macos-replicated:
	$(MACOS_COMMAND_REPLICATED)


clean:
	rm -f libdiehard.so libdiehard_r.so libdiehard.dylib



