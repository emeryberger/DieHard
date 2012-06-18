DieHard
=======

DieHard: An error-resistant memory allocator for Windows, Linux, and Mac OS X

------------------------------------------
DieHard / Exterminator / DieHarder

Copyright (C) 2005-2012 by [Emery Berger](http://www.cs.umass.edu/~emery)  
University of Massachusetts, Amherst  
 
<http://www.diehard-software.org>  
<http://www.die-harder.org>  

----------------------------------------

This distribution includes the source for three systems:

   + **DieHard**: a system that increases RELIABILITY by allowing programs
   with memory errors to run correctly, with high probability.

   DieHard was the direct inspiration for the Fault-Tolerant Heap
   incorporated in Windows 7, though it goes far beyond it in terms of
   reliability.

   DieHard automatically hardens software applications against a wide
   range of bugs. These bugs -- known as memory errors -- often end up
   as serious security vulnerabilities, cause crashes, or lead to
   unpredictable behavior. DieHard either eliminates these bugs
   altogether, or avoids them with high probability. In other words,
   DieHard can take some buggy programs and make them bug-free (or close
   to it).

   DieHard was first described in the Berger and Zorn PLDI 2006 paper
   [DieHard: Probabilistic Memory Safety for Unsafe
   Languages](docs/pldi2006-diehard.pdf) ([ACM
   link](http://doi.acm.org/10.1145/1133981.1134000)), though the
   DieHarder paper presents a more up-to-date description.

   + **Exterminator**: a system that *AUTOMATICALLY FIXES* programs with memory errors.

   Exterminator builds on DieHard (in fact, using a variant called
   **DieFast**, and uses statistical inference to locate and fix memory
   errors. Exterminator first appeared in the PLDI 2007 paper [Exterminator: automatically correcting memory errors with high probability](docs/pldi028-novark.pdf)([ACM link](http://doi.acm.org/10.1145/1250734.1250736)),
    and Communications of the ACM featured
   a more concise version as [a technical highlight](docs/acm-exterminator.pdf) in December 2008 ([ACM link](http://doi.acm.org/10.1145/1409360.1409382)).

   + **DieHarder**: a system that improves SECURITY by protecting
   programs against security exploits. This work presents an
   analytical framework for determining the security of memory
   allocators against attacks, and presents a version of DieHard that
   (as far as we know) is the most secure memory allocator. DieHarder
   first appeared in the [Proceedings of the 17th ACM Conference on
   Computer and Communications
   Security](http://doi.acm.org/10.1145/1866307.1866371) (CCS 2010).


----------------------------
Build and usage instructions
----------------------------

### Windows ###

To build the Windows version (randomized runtime only), do this:

	% nmake /f Makefile.win32

To use DieHard as a library with your application, link your program
as follows:

	% cl /MD yourapp.cpp usewinhard.obj winhard.lib

Copy `winhard.dll` to the same directory as the executable.


### Linux / Solaris ###

Build the shared library with `make`. You can either link in the
resulting shared object (`libdiehard.so`), or use DieHard by
setting the `LD_PRELOAD` environment variable, as in:

	% setenv LD_PRELOAD /path/to/diehard/libdiehard.so

To use the replicated version, invoke your program with (for example):

	% diehard 3 /path/to/libdiehard_r.so yourapp

This would create 3 replicas of yourapp. If the application does not
read from standard input, add `< /dev/null` to the command line.

### Mac OS X ###

To use DieHard, build with "make darwin" and set two environment variables
as follows:

	% export DYLD_INSERT_LIBRARIES=/path/to/libdiehard.dylib
	% export DYLD_FORCE_FLAT_NAMESPACE=

DieHard will then replace the system malloc in any new application executed
from that terminal window.

----------------------------
Notes about the source code:
----------------------------

This directory contains the source code for DieHard. The version here
uses an adaptive algorithm that differs substantially from that
described in the PLDI 2006 paper (../docs/pldi2006-diehard.pdf). In
particular, this version adjusts its heap size dynamically rather than
relying on a static, a priori heap choice.

The original version, as described in the PLDI 2006 paper, is in the
subdirectory `static/`. In addition, the support for replication is in
the subdirectory `replicated/`.

The fault injectors described in the PLDI paper are in the `util/`
directory. The library `libbrokenmalloc.so` can be used to inject buffer
overflows and dangling pointer errors. To inject buffer overflows,
just set `LD_PRELOAD` to point to `libbrokenmalloc.so`, and set the
appropriate environment variables (shown at startup). To inject
dangling pointer errors, you must first run the program with
`libtrackalloc.so` preloaded, and then run it on the same inputs with
`libbrokenmalloc.so`.
