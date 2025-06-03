THIS CODE SHOULD BE CONSIDERED MORIBUND - IT IS NO LONGER DEVELOPED OR MAINTAINED

OpenUH Overview
---------------

OpenUH is a branch of the Open64 repository, focusing on compiler
implementation for parallel programming models and support for parallel
program analysis tools.

OpenUH 3.0 is the latest release of the OpenUH compiler from the HPCTools
Group at the University of Houston. It is based on the Open64 5.0 compiler
infrastructure, and provides a number of features to facilitate parallel
programming for HPC application development. The OpenUH 3.0 source code may be
downloaded on the web, or retrieved from the Open64.net public subversion
repository. Prebuilt installations of OpenUH 3.0 are also available online. 

OpenMP
--------
OpenUH extends the Open64 OpenMP implementation by adding support for nested
parallelism, OpenMP 3.0 explicit tasking, and other OpenMP bug fixes. The
OpenMP runtime library that comes with OpenUH 3.0 supports several task
scheduling strategies, enables selection of more scalable barrier algorithms,
and provides an implementation of the OpenMP Collector API for interaction
with performance collection tools. 

Use the omprun script to control varoius parameters for the execution of your
OpenMP programs. 

Coarray Fortran
----------------
OpenUH 3.0 provides support for Coarray Fortran (CAF), an extension that has
been adopted in the Fortran 2008 standard. With the use of coarrays, a
programmer can easily write parallel Fortran programs for distributed systems.
CAF provides an array extension for carrying out 1-sided communication
between executing "images", as well as a number of intrinsic functions for
synchronization between all or a specified subset of the images. The
OpenUH 3.0 CAF implementation can work in conjunction with either the GASNet
or ARMCI runtime libraries, open-source projects which are freely downloadable
online.

Use the uhcaf and cafrun scripts to compile and run your CAF programs,
respectively.

Dragon
----------
Dragon is a compiler tool for visualizing call graph, control-flow graph,
array region accesses, and more. OpenUH 3.0 can generate analysis files that
Dragon can load and present to the user.

To use with Dragon, compile with -dragon. The Dragon tool will be available at
the HPCTools group website for download. 


Open64 Merge History:
---------------------

[13]  merged r3826:4037 of Open64 main trunk. (2/28/13)
[12]  merged r3777:3826 of Open64 main trunk. (6/17/12)
[11]  merged r3741:3777 of Open64 main trunk. (11/14/11)
[10]  merged r3669:3741 of Open64 main trunk. (9/23/11)
[9]   merged r3640:3669 of Open64 main trunk. (7/1/11)
[8]   merged r3556:3663 of openmp3.0 branch. (7/1/11)
[7]   merged r3610:3640 of Open64 main trunk. (6/4/11)
[6]   merged r3590:3610 of Open64 main trunk. (5/19/11)
[5]   merged r3579:3590 of Open64 main trunk. (5/8/11)
[4]   merged r3576:3579 of Open64 main trunk. (5/3/11)
[3]   merged r3477:3556 of openmp3.0 branch. (5/1/11)
[2]   merged r3520:3576 of Open64 main trunk. (4/30/11)
[1]   merged through 3520 of Open64 main trunk. (4/30/11)


Notes:

Merge [13] merges several months worth of commits from Open64 5.x main trunk
into OpenUH. This spans numerous bug fixes in LNO, WOPT, and CG, as well as
several new features. Included is a patch for x6-ppc32 cross compiling
support, CGSSA implementation, auto-detection of CPU on the host system for a
more accurate target architecture, correction of ipa-link arguments (for
debugging),  allowing debugging of sources compiled from different directory,
supporting variable length arrays in structs, removing some unused PROMPF
implementation code, updates to CG scheduler, removal of unused GCC 4.0 C/C++
front-end, a new perl version of kopencc script, some misc.  transformations
to enable various loop optimizations, fixing build errors when compiling with
gcc 4.7, and a configure-based build system for PPC native compiler. The
removal of PROMPF is something we may try to reverse in the future, because
some analyses implemented in OpenUH several years ago used it.  Open64 bugs
fixed by this merge are: 362, 778-779, 783, 785, 787, 798, 830, 889, 891, 897,
903, 908, 910-912, 924, 929, 933-934, 938-941, 943-944, 947-952, 954-955,
962-963, 966-967, 969, 970-972, 978, 999, 1003-1004, 1800 (from Open64
bugzilla).

Merge [12] merges some early features added to Open64 5.0 from the open64 main
trunk. This includes improvements for GNU compatibility, Nystrom alias
analyzer in IPA phase, and a new siloed reference analysis pass in IPA.

Merge [11] merges the rest of Open64 5.0 release into OpenUH.

For merge [10], a number of fixes/enhancements over a 2 month period were
integrated into OpenUH. Enhancements include IF-statement vectorization
framework, CG updates for AMD Bulldozer, enabling more if-conversion in WOPT,
option to disable shared library support for improved portability (e.g. to
port to Cygwin/Windows), VCG graph support for procedure CFGs in CG, a
'copyin-copyout" optimization for structure members whose accesses exhibit
poor cache locality, new DSP Zero-Delay-Loop (ZDL) implementation. Fixes
for sqrt intrinsic, memory leaks, CODEREP:Set_dtyp_const_val(dt,v),
superfluous region exit blocks, EBO mul operation, integer division
simplification, volatile fields, removing obsolete KEY preprocessor and PURPLE
feature, WOPT seg fault in CFG phase and more also included. 

For merge [8], still need to confirm that default(shared) works correctly on
task constructs. 

For merge [5], commit 3590 addresses the issue encountered in targ_const.cxx
during merge [2] (see note below). I re-enabled the assertion check. See the
open64 commit log for more details. -- Deepak

For merge [2], I turned off some assertion checks in
common/com/x8664/targ_const.cxx, because they were causing the bootstrapped
library build to fail with --with-build-optimize=DEBUG. The same problem was
observed for Open64 main trunk, as of revision r3576. --Deepak
