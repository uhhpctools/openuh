#!/bin/bash

#  Generate CO_PRODUCT routines.
#
#  Copyright (C) 2014 University of Houston.
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

# all-to-all parallel reduction
write_all2all()
{
    local ty1=$1
    local ty2="$2 $3"
    local indent="       "
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!!!!!"
    echo -e "$indent   ! $ty1 CO_PRODUCT_ALL2ALL"
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!!!!!"
    echo ""
     for d in `seq 0 7`; do
        echo -e  "$indent  subroutine CO_PRODUCT_ALL2ALL_"$ty1"_$d(source, result)"
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
        echo -e "$indent    result=1"
        echo -e "$indent    do i=1,num_images()"
        echo -e -n "$indent      result = result * source"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo "[i]"
        echo -e "$indent    end do"
        echo -e "$indent    sync all"
        echo -e "$indent  end subroutine CO_PRODUCT_ALL2ALL_"$ty1"_$d"
        echo ""
    done
}

# tree-based reduction using sync all barrier
write_2tree_syncall()
{
    local ty1=$1
    local ty2="$2 $3"
    local indent="       "
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo -e "$indent   ! $ty1 CO_PRODUCT_2TREE_SYNCALL"
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo ""
     for d in `seq 0 7`; do
        echo -e  "$indent  subroutine CO_PRODUCT_2TREE_SYNCALL_"$ty1"_$d(source, result)"
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
        echo -n ", result"
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
        echo -e -n "$indent    $ty2, allocatable :: buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo  "[:]"
        echo ""
        echo -e "$indent    integer :: k,ti,ni"
        if [ $d -gt 0 ]; then
            echo -n "$indent    integer :: x1"
        fi
        for i in `seq 2 $d`; do
            echo -n ",x$i"
        done
        echo ""

        echo -e "$indent    ti = this_image()"
        echo -e "$indent    ni = num_images()"
        for i in `seq 1 $d`; do
            echo -e "$indent    x$i = ubound(source,$i)-lbound(source,$i)+1"
        done
        echo -n "$indent    allocate( buf"
        if [ $d -gt 0 ]; then
            echo -n "(1:x1"
        fi
        for i in `seq 2 $d`; do
            echo -n ",1:x$i"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -n "[*] )"
        echo ""
        echo -e "$indent    buf = source"
        echo -e "$indent    sync all"
        echo -e "$indent    k = 1"

        echo -e "$indent    do while (k < ni)"
        echo -e "$indent        if ( mod(ti-1,2*k)==0) then"
        echo -e "$indent            if ((ti+k)<=ni) then"
        echo -n "$indent                buf = buf * buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -e "[ti+k]"
        echo -e "$indent            end if"
        echo -e "$indent        end if"
        echo -e "$indent        sync all"
        echo -e "$indent        k = k*2"
        echo -e "$indent    end do"

        echo -e "$indent    k = k/2"
        echo -e "$indent    do while (k > 0)"
        echo -e "$indent        if ( mod(ti-1,k)==0) then"
        echo -e "$indent            if ( mod(ti-1,2*k)==0) then"
        echo -e "$indent                if ((ti+k)<=ni) then"
        echo -n "$indent                    buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -e "[ti+k] = buf"
        echo -e "$indent                end if"
        echo -e "$indent            end if"
        echo -e "$indent        end if"
        echo -e "$indent        sync all"
        echo -e "$indent        k = k/2"
        echo -e "$indent    end do"

        echo -e "$indent    result = buf"
        echo -e "$indent    deallocate(buf)"

        echo -e "$indent  end subroutine CO_PRODUCT_2TREE_SYNCALL_"$ty1"_$d"
        echo ""
    done
}

# tree-based reduction using sync images for point-to-point synchronization
write_2tree_syncimages()
{
    local ty1=$1
    local ty2="$2 $3"
    local indent="       "
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!"
    echo -e "$indent   ! $ty1 CO_PRODUCT_2TREE_SYNCIMAGES"
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!"
    echo ""
     for d in `seq 0 7`; do
        echo -e  "$indent  subroutine CO_PRODUCT_2TREE_SYNCIMAGES_"$ty1"_$d(source, result)"
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
        echo -n ", result"
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
        echo -e -n "$indent    $ty2, allocatable :: buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo  "[:]"
        echo ""
        echo -e "$indent    integer :: k,ti,ni"
        if [ $d -gt 0 ]; then
            echo -n "$indent    integer :: x1"
        fi
        for i in `seq 2 $d`; do
            echo -n ",x$i"
        done
        echo ""

        echo -e "$indent    ti = this_image()"
        echo -e "$indent    ni = num_images()"
        for i in `seq 1 $d`; do
            echo -e "$indent    x$i = ubound(source,$i)-lbound(source,$i)+1"
        done
        echo -n "$indent    allocate( buf"
        if [ $d -gt 0 ]; then
            echo -n "(1:x1"
        fi
        for i in `seq 2 $d`; do
            echo -n ",1:x$i"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -n "[*] )"
        echo ""
        echo -e "$indent    buf = source"
        echo -e "$indent    sync all"
        echo -e "$indent    k = 1"

        echo -e "$indent    do while (k < ni)"
        echo -e "$indent        if ( mod(ti-1,2*k)==0) then"
        echo -e "$indent            if ((ti+k)<=ni) then"
        echo -n "$indent                buf = buf * buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -e "[ti+k]"
        echo -e "$indent            end if"
        echo -e "$indent            if (mod(ti-1,4*k)==0) then"
        echo -e "$indent                if ((ti+2*k)<=ni) then"
        echo -e "$indent                    sync images(ti+2*k)"
        echo -e "$indent                end if"
        echo -e "$indent            else"
        echo -e "$indent                if ((ti-2*k)>=1) then"
        echo -e "$indent                    sync images(ti-2*k)"
        echo -e "$indent                end if"
        echo -e "$indent            end if"
        echo -e "$indent        end if"
        echo -e "$indent        k = k*2"
        echo -e "$indent    end do"

        echo -e "$indent    k = k/2"
        echo -e "$indent    do while (k > 0)"
        echo -e "$indent        if ( mod(ti-1,k)==0) then"
        echo -e "$indent            if ( mod(ti-1,2*k)==0) then"
        echo -e "$indent                if ((ti+k)<=ni) then"
        echo -n "$indent                    buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -e "[ti+k] = buf"
        echo -e "$indent                end if"
        echo -e "$indent                if ((ti+k)<=ni) then"
        echo -e "$indent                    sync images(ti+k)"
        echo -e "$indent                end if"
        echo -e "$indent            else"
        echo -e "$indent                if ((ti-k)>=1) then"
        echo -e "$indent                    sync images(ti-k)"
        echo -e "$indent                end if"
        echo -e "$indent            end if"
        echo -e "$indent        end if"
        echo -e "$indent        k = k/2"
        echo -e "$indent    end do"

        echo -e "$indent    result = buf"
        echo -e "$indent    deallocate(buf)"

        echo -e "$indent  end subroutine CO_PRODUCT_2TREE_SYNCIMAGES_"$ty1"_$d"
        echo ""
    done
}

# tree-based reduction using events for point-to-point synchronization
write_2tree_events()
{
    local ty1=$1
    local ty2="$2 $3"
    local indent="       "
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo -e "$indent   ! $ty1 CO_PRODUCT_2TREE_EVENTS"
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo ""
     for d in `seq 0 7`; do
        echo -e  "$indent  subroutine CO_PRODUCT_2TREE_EVENTS_"$ty1"_$d(source, result)"
        echo -e "              type :: EVENT_TYPE"
        echo -e "                  integer(kind=8) :: e = 0"
        echo -e "              end type EVENT_TYPE"
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
        echo -n ", result"
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
        echo -e -n "$indent    $ty2, allocatable :: buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo  "[:]"
        echo ""
        echo -e "$indent    integer :: l,k,ti,ni,li,ri,ne"
        if [ $d -gt 0 ]; then
            echo -n "$indent    integer :: x1"
        fi
        for i in `seq 2 $d`; do
            echo -n ",x$i"
        done
        echo ""
        echo -e "$indent    type(event_type),allocatable :: ev(:)[:]"

        echo -e "$indent    ti = this_image()"
        echo -e "$indent    ni = num_images()"
        echo -e "$indent    li = log2_images()"
        echo -e "$indent    ri = rem_images()"
        echo -e "$indent    ne = li"
        echo -e "$indent    if (ri /= 0) ne = ne + 1"
        for i in `seq 1 $d`; do
            echo -e "$indent    x$i = ubound(source,$i)-lbound(source,$i)+1"
        done
        echo -e "$indent    allocate( ev(ne)[*] )"
        echo -n "$indent    allocate( buf"
        if [ $d -gt 0 ]; then
            echo -n "(1:x1"
        fi
        for i in `seq 2 $d`; do
            echo -n ",1:x$i"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -n "[*] )"
        echo ""
        echo -e "$indent    buf = source"
        echo -e "$indent    k = 1"
        echo -e "$indent    l = 1"

        echo -e "$indent    do while (k < ni)"
        echo -e "$indent        if ( mod(ti-1,2*k)==0) then"
        echo -e "$indent            if ((ti+k)<=ni) then"
        echo -e "$indent                event wait(ev(l))"
        echo -n "$indent                buf = buf * buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -e "[ti+k]"
        echo -e "$indent            end if"
        echo -e "$indent        else"
        echo -e "$indent            if ((ti-k)>=1) then"
        echo -e "$indent                event post(ev(l)[ti-k])"
        echo -e "$indent            end if"
        echo -e "$indent        end if"
        echo -e "$indent        k = k*2"
        echo -e "$indent        l = l+1"
        echo -e "$indent    end do"

        echo -e "$indent    l = ne"
        echo -e "$indent    k = k/2"
        echo -e "$indent    do while (k > 0)"
        echo -e "$indent        if ( mod(ti-1,k)==0) then"
        echo -e "$indent            if ( mod(ti-1,2*k)==0) then"
        echo -e "$indent                if ((ti+k)<=ni) then"
        echo -n "$indent                    buf"
        if [ $d -gt 0 ]; then
            echo -n "(:"
        fi
        for i in `seq 2 $d`; do
            echo -n ",:"
        done
        if [ $d -gt 0 ]; then
            echo -n ")"
        fi
        echo -e "[ti+k] = buf"
        echo -e "$indent                end if"
        echo -e "$indent                if ((ti+k)<=ni) then"
        echo -e "$indent                    event post(ev(l)[ti+k])"
        echo -e "$indent                end if"
        echo -e "$indent            else"
        echo -e "$indent                if ((ti-k)>=1) then"
        echo -e "$indent                    event wait(ev(l))"
        echo -e "$indent                end if"
        echo -e "$indent            end if"
        echo -e "$indent        end if"
        echo -e "$indent        k = k/2"
        echo -e "$indent        l = l-1"
        echo -e "$indent    end do"

        echo -e "$indent    result = buf"
        echo -e "$indent    deallocate(buf)"
        echo -e "$indent    deallocate(ev)"

        echo -e "$indent  end subroutine CO_PRODUCT_2TREE_EVENTS_"$ty1"_$d"
        echo ""
    done
}

alg=$1

if [ "$alg" != "" ]; then
    case "$alg" in
        all2all)
            ;;
        2tree_syncall)
            ;;
        2tree_syncimages)
            ;;
        2tree_events)
            ;;
        *)
            echo "unknown algorithm: " $alg >&2
            exit 1
            ;;
    esac
else
    alg="all"
fi

if [ "$alg" == "all2all" -o "$alg" == "all" ]; then
    echo "" > co_product_all2all.caf
    for p in 1 2 4 8; do
        ty1="INT$p"
        ty2="integer (kind=$p)"
        write_all2all $ty1 $ty2 >> co_product_all2all.caf
    done

    for p in 4 8; do
        ty1="REAL$p"
        ty2="real (kind=$p)"
        write_all2all $ty1 $ty2 >> co_product_all2all.caf
    done

    for p in 4 8; do
        ty1="C$p"
        ty2="complex (kind=$p)"
        write_all2all $ty1 $ty2 >> co_product_all2all.caf
    done
fi

if [ "$alg" == "2tree_syncall" -o "$alg" == "all" ]; then
    echo "" > co_product_2tree_syncall.caf
    for p in 1 2 4 8; do
        ty1="INT$p"
        ty2="integer (kind=$p)"
        write_2tree_syncall $ty1 $ty2 >> co_product_2tree_syncall.caf
    done

    for p in 4 8; do
        ty1="REAL$p"
        ty2="real (kind=$p)"
        write_2tree_syncall $ty1 $ty2 >> co_product_2tree_syncall.caf
    done

    for p in 4 8; do
        ty1="C$p"
        ty2="complex (kind=$p)"
        write_2tree_syncall $ty1 $ty2 >> co_product_2tree_syncall.caf
    done
fi

if [ "$alg" == "2tree_syncimages" -o "$alg" == "all" ]; then
    echo "" > co_product_2tree_syncimages.caf
    for p in 1 2 4 8; do
        ty1="INT$p"
        ty2="integer (kind=$p)"
        write_2tree_syncimages $ty1 $ty2 >> co_product_2tree_syncimages.caf
    done

    for p in 4 8; do
        ty1="REAL$p"
        ty2="real (kind=$p)"
        write_2tree_syncimages $ty1 $ty2 >> co_product_2tree_syncimages.caf
    done

    for p in 4 8; do
        ty1="C$p"
        ty2="complex (kind=$p)"
        write_2tree_syncimages $ty1 $ty2 >> co_product_2tree_syncimages.caf
    done
fi

if [ "$alg" == "2tree_events" -o "$alg" == "all" ]; then
    echo "" > co_product_2tree_events.caf
    for p in 1 2 4 8; do
        ty1="INT$p"
        ty2="integer (kind=$p)"
        write_2tree_events $ty1 $ty2 >> co_product_2tree_events.caf
    done

    for p in 4 8; do
        ty1="REAL$p"
        ty2="real (kind=$p)"
        write_2tree_events $ty1 $ty2 >> co_product_2tree_events.caf
    done

    for p in 4 8; do
        ty1="C$p"
        ty2="complex (kind=$p)"
        write_2tree_events $ty1 $ty2 >> co_product_2tree_events.caf
    done
fi
