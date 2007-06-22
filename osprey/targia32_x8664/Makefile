#
#
#  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
#
#  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it would be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
#
#  Further, this software is distributed without any warranty that it is
#  free of the rightful claim of any third person regarding infringement 
#  or the like.  Any license provided herein, whether implied or 
#  otherwise, applies only to this software file.  Patent licenses, if 
#  any, provided herein do not apply to combinations of this program with 
#  other software, or any other product whatsoever.  
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write the Free Software Foundation, Inc., 59
#  Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
#  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
#  Mountain View, CA 94043, or:
#
#  http://www.sgi.com
#
#  For further information regarding this notice, see:
#
#  http://oss.sgi.com/projects/GenInfo/NoticeExplan
#
#

#
#  Makefile for targia32_ia64_nodebug
#

#
#  These directories must be "built" first because they generate header files
#  which other directories depend on.
#
INCLUDE_SUBDIRS := \
	include \
	targ_info

#
#  Next, all of the libraries are built because the tools depend on them.
#
LIBRARY_SUBDIRS := \
	arith \
	libcif \
	libcmplrs \
	libcomutil \
	libelf \
	libdwarf \
	libelfutil \
	libiberty \
	libfortran \
	libu \
	libmv \
	libmsgi \


#
#  Finally, the compiler tools (components) are built.
#
TOOL_SUBDIRS := \
	be \
	cg \
	driver \
	g++fe \
	gccfe \
	ipa \
	ipl \
	ir_tools \
	lno \
	lw_inline \
	whirl2c \
	whirl2f \
	wopt \
	wgen \
	crayf90 \
	libopenmp \
	#f95_coco 


SUBDIRS := $(INCLUDE_SUBDIRS) $(LIBRARY_SUBDIRS) $(TOOL_SUBDIRS)

default clean clobber:
	+@for d in $(SUBDIRS); do \
	    echo "MAKE   $@ in $(PWD)/$$d"; \
	    $(MAKE) $(if $V,,--no-print-directory) -C $$d $@; \
	    retval=$$?; \
	    if [ $$retval != 0 ]; then \
		if grep -q k <<<'$(MAKEFLAGS)'; then \
		    exit=$$retval; \
		else \
		    exit $$retval; \
		fi; \
	    fi; \
	done; \
	exit $$exit
