# -*- Makefile -*-
#
#  Copyright 2005 PathScale, Inc.  All Rights Reserved.

#
#  Define build parameters
#
BUILD_AREA     = ..
BUILD_TOT      = ../..
BUILD_BASE     = ../../libopenmp
BUILD_ROOTS    = TARGET
BUILD_ABI      = 64BIT
BUILD_ARCH     = X8664
BUILD_TYPE     = SHARED
BUILD_BOTH     = typically
BUILD_OPTIMIZE = NODEBUG
BUILD_VARIANT  = DEFAULT
BUILD_INSTALL  = DEFAULT
BUILD_TARGET   = X8664
BUILD_OS       = LINUX
BUILD_HOST     = IA32

#  Include (optional) parameter override file
-include $(BUILD_AREA)/Makefile.override

#  Include setup file which will then include Makefile.base
include $(BUILD_TOT)/Makefile.gsetup

# Local override to really using gcc for 64-bit build
# CCNAME = gcc -m64
