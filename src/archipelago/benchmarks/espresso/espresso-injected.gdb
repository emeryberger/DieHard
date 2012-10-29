symbol-file ~/work/plasma/sparseheaps/brokenmalloc/libbrokenmalloc.so
file ~/work/plasma/sparseheaps/benchmarks/espresso/espresso
set args -t ~/work/plasma/sparseheaps/benchmarks/espresso/Input/largest.espresso
set environment LD_PRELOAD /home/vlvin/work/plasma/sparseheaps/brokenmalloc/libbrokenmalloc.so:/home/vlvin/work/plasma/sparseheaps/libsparseheaps.so
set environment INJECTED_UNDERFLOW_AMOUNT 4
set environment INJECTED_UNDERFLOW_FREQUENCY 0.0

