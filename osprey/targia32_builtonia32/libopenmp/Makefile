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
BUILD_ABI      = I32BIT
BUILD_ARCH     = IA32
BUILD_TYPE     = SHARED
BUILD_BOTH     = quite
BUILD_OPTIMIZE = NODEBUG
BUILD_VARIANT  = DEFAULT
BUILD_INSTALL  = DEFAULT
BUILD_TARGET   = IA32
BUILD_OS       = LINUX
BUILD_HOST     = IA32

#  Include (optional) parameter override file
-include $(BUILD_AREA)/Makefile.override

#  Include setup file which will then include Makefile.base
include $(BUILD_TOT)/Makefile.gsetup
