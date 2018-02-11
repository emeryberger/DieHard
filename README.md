__if downloading from GitHub, make sure to use `--recursive`, as in:__
    `git clone --recursive https://github.com/emeryberger/DieHard`

DieHard
=======

DieHard: An error-resistant memory allocator for Windows, Linux, and Mac OS X

------------------------------------------
DieHard / Exterminator / DieHarder

Copyright (C) 2005-2018 by [Emery Berger](http://www.emeryberger.com/)  
University of Massachusetts Amherst  

<http://www.emeryberger.com>  
<http://www.diehard-software.org>  
<http://www.die-harder.org>  

----------------------------------------

This distribution includes the source for three systems.

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
   DieHarder directly inspired security hardening features in Microsoft
   Windows (starting with version 8).


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


Frequently Asked Questions
--------------------------

### What does DieHard do? ###

DieHard prevents some nasty errors - ones that crash programs and lead
to security vulnerabilities. These are memory errors, including
double-frees & heap corruption (which DieHard eliminates), and
dangling pointer errors (or stale pointers) and heap buffer overflows
(which DieHard makes unlikely to have any effect).  Who should use
DieHard?

DieHard is good for software developers, since it makes programmer
errors unlikely to crash a program and reduces the risk of security
vulnerabilities. It's also good for end users, who can take advantage
of DieHard's protections now.  What versions of Windows (& Firefox)
does DieHard support?

In addition to supporting nearly any application for Linux and
Solaris, DieHard currently protects Firefox on Windows XP and
2003. DieHard works with Firefox versions 1.5.0.9 and higher, as well
as version 2.0.0.1.

### Can DieHard protect any other application than Mozilla? ###

On Windows, the DieHard protection system currently supports Mozilla
only; we plan to add more applications soon. On Linux, DieHard can
protect any application. In addition, programmers using DieHard (on
Windows or Linux) can protect any application they are developing.

### My security program claims that the DieHard zip file contains a virus - can that be true? ###

Absolutely not. Your security program (so far, I only know of one, by
"astaro") noticed that the distribution contains an example HTML file
that shows how DieHard works. And that HTML does not contain a virus,
either! It just causes certain versions of Mozilla to crash if you
aren't running DieHard.

### Does running DieHard significantly slow down a system? ###

Unless your system had too little memory installed, DieHard generally has no
perceptible performance impact on applications like Firefox.

### How much more memory does DieHard require? ###

That depends on your application, but in general, memory consumption will increase somewhat.

### Does DieHard protect only programs launched after it is activated? ###

Yes.

### I see that DieHard runs multiple copies of a program and "votes". How many copies of a program are running at the same time? ###

There is a version of DieHard for Linux that runs multiple replicas
simultaneously, and then you can choose how many replicas you would
like to run. However, the Windows version runs just one copy of your
program.
 
### Does DieHard prevent all crashes? If not, what does it prevent? ###

No, although that would be nice. DieHard completely prevents
particular memory management errors from having any effect (these are
"double frees" and "invalid frees"). It dramatically reduces the
likelihood of another kind of error known as "dangling pointer"
errors, and lowers the odds that moderate buffer overflows will have
any effect. It prevents certain library-based heap overflows (e.g.,
through `strcpy`), and all but eliminates another problem known as "heap
corruption."

### How does DieHard differ from Vista's, OpenBSD's, and Linux's "address space randomization"? ###

Address space randomization places large chunks of memory (obtained
via `mmap` / `VirtualAlloc`) at different places in memory, but leaves
unchanged the relative position of heap objects. Linux adds some
checks for particular memory management errors (and then aborts the
program).

Longer technical answer: OpenBSD (a variant of PHKmalloc) does some of
what DieHard's allocator does, but DieHard does much more. On the
security side, DieHard adds much more "entropy"; on the reliability
side, it mathematically reduces the risk that a programmer bug will
have any impact on program execution.

OpenBSD randomly locates pages of memory and allocates small objects
from these pages. It improves security by avoiding the effect of
certain errors. Like DieHard, it is resilient to double and invalid
frees. It places guard pages around large chunks and frees such large
chunks back to the OS (causing later references through dangling
pointers to fail unless the chunk is reused). It attempts to block
some buffer overflows by using page protection. Finally, it shuffles
some allocated objects around on a page, randomizing their location
within a page.

DieHard goes much further. First, it completely segregates heap
metadata from the heap, making heap corruption (and hijack attacks)
nearly impossible. On OpenBSD, a large-enough underflow on OpenBSD can
overwrite the page directory or local page info struct (at the
beginning of each page), hijacking the allocator. By contrast, none of
DieHard's metadata is located in the allocated object space.

Second, DieHard randomizes the placement of objects across the entire
heap. This has numerous advantages. On the security side, it makes
brute-force attempts to locate adjacent objects nearly impossible --
in OpenBSD, knowing the allocation sequence determines which pages
objects will land on (see the presentation pointed to above).

DieHard's complete randomization is key to provably avoiding a range
of errors with high probability. It reduces the worst-case odds that a
buffer overflow has any impact to 50%. The actual likelihood is even
lower when the heap is not full. DieHard also avoids dangling pointer
errors with very high probability (e.g., 99.999%), making it nearly
impervious to such mistakes. You can read the PLDI paper for more
details and formulae.


Acknowledgements
----------------

This work is supported in part by the National Science Foundation,
Intel Corporation, and Microsoft Research. This material is based upon
work supported by the National Science Foundation under Grant No
CNS-0615211. Any opinions, findings and conclusions or recommendations
expressed in this material are those of the author(s) and do not
necessarily reflect the views of the National Science Foundation
(NSF).