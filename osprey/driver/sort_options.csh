#!/bin/csh -f
#
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
# $1 == source dir, $2 == OPTIONS file
# $3 == optional cpp defines
# need C locale for sort to work as expected
setenv LANG C
set dir = $1
set file = $2
shift
shift
/lib/cpp -traditional -P $* $dir/$file > tmp.options.cpp
awk -f $dir/sort_options.awk tmp.options.cpp
# note that some linux versions of sort are broken,
# so we actually re-sort within table for safety.
sort -r tmp.options > tmp.options.sort
echo "%%% OPTIONS"
cat tmp.options.sort
echo "%%% COMBINATIONS"
cat tmp.options.combo
rm -f tmp.options*
