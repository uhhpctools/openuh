#!/bin/bash

#  Generate CO_MAXVAL routines.
#
#  Copyright (C) 2012 University of Houston.
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
#  Contact information:
#  http://www.cs.uh.edu/~hpctools
#


write_functions()
{
    local ty1=$1
    local ty2="$2 $3"
    local indent="       "
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!"
    echo -e "$indent   ! $ty1 CO_MAXVAL"
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!"
    echo ""
     for d in `seq 0 7`; do
        echo -e  "$indent  subroutine CO_MAXVAL_"$ty1"_$d(source, result)"
        echo -e -n "$indent    $ty2 :: source"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi 
        for i in `seq 2 $d`; do
            echo -n ",:"
        done 
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi 
        echo -n "[*], result"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi 
        for i in `seq 2 $d`; do
            echo -n ",:"
        done 
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi 
        echo ""
        echo -e "$indent    integer :: i"
        echo ""
        echo -e "$indent    sync all"
        echo -e "$indent    result=-1*huge(result)-1"
        echo -e "$indent    do i=1,num_images()"
        echo -e -n "$indent      result = max(result,1*source"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi 
        for i in `seq 2 $d`; do
            echo -n ",:"
        done 
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi 
        echo "[i])"
        echo -e "$indent    end do"
        echo -e "$indent    sync all"
        echo -e "$indent  end subroutine CO_MAXVAL_"$ty1"_$d"
        echo ""
    done 
}

for p in 1 2 4 8; do
    ty1="INT$p"
    ty2="integer (kind=$p)"
    write_functions $ty1 $ty2
done 

for p in 4 8; do
    ty1="REAL$p"
    ty2="real (kind=$p)"
    write_functions $ty1 $ty2
done 

# for p in 4 8; do
#     ty1="C$p"
#     ty2="complex (kind=$p)"
#     write_functions $ty1 $ty2
# done 
