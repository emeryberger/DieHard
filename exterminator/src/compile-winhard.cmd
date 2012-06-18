@echo off
echo Building winhard.dll.
cl /MD /D_MT=1 /DDIEHARD_REPLICATED=0 /DDIEHARD_MULTITHREADED=1 /nologo /Ox /Oy /DNDEBUG /c winhard.cpp
cl /MD /D_MT=1 /nologo /Ox /Oy /DNDEBUG /c usewinhard.cpp
cl /LD /D_MT=1 winhard.obj /link /base:0x63000000 kernel32.lib msvcrt.lib /subsystem:console /dll /incremental:no /entry:HardDllMain
echo *****
echo Build complete.

