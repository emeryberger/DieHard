@echo off
echo Building windiehard.dll.
cl /Zp8 /Zi /nologo /W2 /WX- /Ox /Ob2 /Oi /Ot /Oy- /D "WIN32" /D "NDEBUG" /arch:AVX /fp:fast /MD /Iheaplayers /Iheaplayers/util /c winwrapper.cpp
: cl /Zp8 /Zi /nologo /W2 /WX- /Ox /Ob2 /Oi /Ot /Oy- /D "WIN32" /D "NDEBUG" /arch:AVX /fp:fast /MD /Iheaplayers /Iheaplayers/util /c wintls.cpp
cl /Zp8 /Zi /nologo /W2 /WX- /Ox /Ob2 /Oi /Ot /Oy- /D "WIN32" /D "NDEBUG" /arch:AVX /fp:fast /MD  /Iheaplayers /Iheaplayers/util /c libdiehard.cpp
cl /Zp8 /Zi /nologo /W2 /WX- /Ox /Ob2 /Oi /Ot /Oy- /D "WIN32" /D "NDEBUG" /arch:AVX /fp:fast /MD /Iheaplayers /Iheaplayers/util /c usewinwrapper.cpp
cl /Zp8 /Zi /nologo /W2 /WX- /Ox /Ob2 /Oi /Ot /Oy- /D "WIN32" /D "NDEBUG" /arch:AVX /fp:fast /MD wintls.obj winwrapper.obj libdiehard.obj /link /out:windiehard.dll /force:multiple /base:0x63000000 kernel32.lib msvcrt.lib /subsystem:console /incremental:no /dll /entry:WinWrapperDllMain
: Embed the manifest in the DLL.
: mt -manifest windiehard.dll.manifest -outputresource:windiehard.dll;2
echo *****
echo Build complete.

