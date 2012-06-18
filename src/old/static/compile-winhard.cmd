@echo off
echo Building winhard.dll.
cl /I.. /G7 /Zi /MD /DDIEHARD_REPLICATED=0 /DDIEHARD_MULTITHREADED=0 /nologo /Ox /Oy /c /DNDEBUG winhard.cpp
cl /I.. /MD /nologo /Ox /Oy /DNDEBUG /c usewinhard.cpp
cl /LD winhard.obj /link /base:0x63000000 kernel32.lib msvcrt.lib /subsystem:console /dll /incremental:no /entry:HardDllMain
echo *****
echo Build complete.

