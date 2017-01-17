# BSP-test-Utilities

================================================================================
= Boot Image Read and Write (Parameters, U-Boot and Uboot Environment File )   =
================================================================================

============
= Building =
============

For native builds, make should be all that's needed.

When cross compiling, CROSS_COMPILE must contain GNU tool prefix,
SYSROOT should be set as appropriate to the tools,
${CROSS_COMPILES}gcc etc. should be in the PATH.  Finally, LINUX
should be set to the directory containing the kernel headers.

       $ LINUX=<Linux Headers> && make

In both cases, make creates binary image in
${CROSS_COMPILE}build.

==============
= Installing =
==============

Just copy image to a location in the PATH.

=========
= Using =
=========

See the built in help using "-h" .
