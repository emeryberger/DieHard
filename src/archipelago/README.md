From "Archipelago: trading address space for reliability and security",
ASPLOS 2008 (Lvin, Novark, Berger and Zorn).

Memory errors are a notorious source of security vulnerabilities that
can lead to service interruptions, information leakage and
unauthorized access. Because such errors are also difficult to debug,
the absence of timely patches can leave users vulnerable to attack for
long periods of time. A variety of approaches have been introduced to
combat these errors, but these often incur large runtime overheads and
generally abort on errors, threatening availability.

This paper presents Archipelago, a runtime system that takes advantage
of available address space to substantially reduce the likelihood that
a memory error will affect program execution. Archipelago randomly
allocates heap objects far apart in virtual address space, effectively
isolating each object from buffer overflows. Archipelago also protects
against dangling pointer errors by preserving the contents of freed
objects after they are freed. Archipelago thus trades virtual address
space---a plentiful resource on 64-bit systems---for significantly
improved program reliability and security, while limiting physical
memory consumption by tracking the working set of an application and
compacting cold objects. We show that Archipelago allows applications
to continue to run correctly in the face of thousands of memory
errors. Across a suite of server applications, Archipelago's
performance overhead is 6% on average (between -7% and 22%), making it
especially suitable to protect servers that have known security
vulnerabilities due to heap memory errors.

@inproceedings{Lvin:2008:ATA:1346281.1346296,
 author = {Lvin, Vitaliy B. and Novark, Gene and Berger, Emery D. and Zorn, Benjamin G.},
 title = {Archipelago: trading address space for reliability and security},
 booktitle = {Proceedings of the 13th international conference on Architectural support for programming languages and operating systems},
 series = {ASPLOS XIII},
 year = {2008},
 isbn = {978-1-59593-958-6},
 location = {Seattle, WA, USA},
 pages = {115--124},
 numpages = {10},
 url = {http://doi.acm.org/10.1145/1346281.1346296},
 doi = {10.1145/1346281.1346296},
 acmid = {1346296},
 publisher = {ACM},
 address = {New York, NY, USA},
 keywords = {Archipelago, buffer overflow, dynamic memory allocation, memory errors, probabilistic memory safety, randomized algorithms, virtual memory},
} 
