symbol-file ../../libsparseheaps.so
file espresso
set args -t Input/largest.espresso
set environment LD_PRELOAD ../../libsparseheaps.so
directory ../..
directory ../../heaplayers
directory ../../util
directory ../../allocator
handle 11 nostop noprint


