#!/bin/bash

write_functions()
{
    local ty1=$1
    local ty2="$2 $3"
    local indent="       "
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!"
    echo -e "$indent   ! $ty1 CO_MINVAL"
    echo -e "$indent   !!!!!!!!!!!!!!!!!!!!!"
    echo ""
     for d in `seq 0 7`; do
        echo -e  "$indent  subroutine CO_MINVAL_"$ty1"_$d(source, result)"
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
        echo -e "$indent    result=huge(result)"
        echo -e "$indent    do i=1,num_images()"
        echo -e -n "$indent      result = min(result,1*source"
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
        echo -e "$indent  end subroutine CO_MINVAL_"$ty1"_$d"
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
