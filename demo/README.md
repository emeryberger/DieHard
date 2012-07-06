This directory contains files that demonstrate DieHard's effectiveness
at preventing memory errors.

The file `disaster.cpp` contains lots of memory errors that cause it
to crash (Linux) or produce unwanted results (Windows). DieHard avoids
all of these.

The file `crash-mozilla.htm` does just what you think it might -- it
crashes Mozilla (versions 1.0.2 and 1.7.3), usually
immediately. DieHard generally prevents these crashes (one is an
overflow, which DieHard avoids probabilistically).

Similarly, the file `crash-firefox-1.0.4.html` does just what it says
-- except when DieHard is running.
