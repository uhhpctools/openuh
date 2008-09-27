#!/bin/bash
#
#
#  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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

VER_MAJOR="4"
VER_MINOR="0"
#PATCH_LEVEL=""
VERSION="${VER_MAJOR}.${VER_MINOR}"

PREBUILT_LIB="./lib"
PREBUILT_BIN="./bin"

# get the machine type
if [ -z "$1" ]; then
    ARCH=`uname -m | sed -e s/i.86/i386/`
else
    ARCH=$1
fi

# set the build host
case $ARCH in 
ia64 )
    BUILD_HOST="ia64"
    TARG_HOST="ia64"
    AREA="osprey/targia64_ia64_nodebug"
    PHASE_DIR_PREFIX="ia64"
    PREBUILD_INTERPOS="ia64-linux"
    INSTALL_TYPE="ia64-native"
    ;;
i386 | x86_64 )
    BUILD_HOST="ia32"
    TARG_HOST="x8664"
    PHASE_DIR_PREFIX="x86_64"
    PREBUILD_INTERPOS="x8664-linux"
    AREA="osprey/targia32_x8664"
    INSTALL_TYPE="x8664-native"
    ;;
cross )
    BUILD_HOST="ia32"
    TARG_HOST="ia64"
    PHASE_DIR_PREFIX="ia64"
    PREBUILD_INTERPOS="ia32-linux"
    AREA="osprey/targia32_ia64_nodebug"
    INSTALL_TYPE="ia64-cross"
    ;;
*)
    echo "Error: Unsupport platform: $ARCH"
    exit 1
    ;;
esac

# get the TOOLROOT
if [ -z ${TOOLROOT} ] ; then
    echo "NOTE: \$TOOLROOT is not set! You can either set \$TOOLROOT or specify an install directory."
    echo "INSTALL DIRECTORY:"
    read	# in $REPLY
    [ ! -d $REPLY ] && echo "$REPLY does not exist. Will create." && mkdir -p $REPLY
    [ ! -d $REPLY ] && echo "Can not create directory: $REPLY, exit." && exit 1
    ORIGIN_DIR=`pwd`
    cd $REPLY
    TOOLROOT=`pwd`
    cd $ORIGIN_DIR
    echo "INSTALL to $TOOLROOT"
fi 

# everything we will install is under $ROOT
ROOT=${TOOLROOT}/

# both targ_os and build_os are 'linux' so far
TARG_OS="linux"
BUILD_OS="linux"

# prepare the source dir
GNUFE_AREA="osprey-gcc/targ${BUILD_HOST}_${TARG_HOST}"
LD_NEW_DIR="osprey/targcygnus_${BUILD_HOST}_${TARG_HOST}/ld"

# prepare the distination dir
INTERPOSE=
[ "$BUILD_HOST" = "$TARG_HOST" ] &&  INTERPOSE="" ; 
PHASEPATH=${ROOT}/${INTERPOSE}/lib/gcc-lib/${PHASE_DIR_PREFIX}-open64-linux/${VERSION}/
NATIVE_LIB_DIR=${PHASEPATH}
BIN_DIR=${ROOT}/${INTERPOSE}/bin
ALT_BIN_DIR=${ROOT}/${INTERPOSE}/altbin
# for omp.h etc. 
INC_DIR=${ROOT}/include

# install commands
INSTALL="/usr/bin/install -D"
INSTALL_DATA="/usr/bin/install -D -m 644"

INSTALL_EXEC_SUB () {

    [ $# -ne 2 ] && echo "!!!Component is missing, you probably need to install prebuilt binaries/archives" && return 1
    
    [ ! -e "$1" ] && echo "$1 does not exist" && return 1

    echo -e "$2 : $1 \n\t${INSTALL} $1 $2\n" | make -f - |\
    grep -v "Entering directory\|Leaving directory\|up to date"

    return 0;
}

INSTALL_DATA_SUB () {

    [ $# -ne 2 ] && echo "!!!Component is missing, you probably need to install prebuilt binaries/archives" && return 1

    [ ! -e "$1" ] && echo "$1 does not exist" && return 1

    echo -e "$2 : $1 \n\t${INSTALL_DATA} $1 $2\n" | make -f - |\
    grep -v "Entering directory\|Leaving directory\|up to date"

    return 0
}

# install the driver
INSTALL_DRIVER () {
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${PHASEPATH}/driver
    INSTALL_EXEC_SUB ${AREA}/driver/kdriver  ${PHASEPATH}/kdriver

    [ ! -d ${BIN_DIR}       ] && mkdir -p ${BIN_DIR}
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/uhcc
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/uhCC
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/uhf90
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/uhf95
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/opencc-${VERSION}
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/openCC-${VERSION}
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/openf90-${VERSION}
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/openf95-${VERSION}
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/orcc
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/orCC
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/orf90
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/orf95
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/opencc
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/openCC
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/openf90
    INSTALL_EXEC_SUB ${AREA}/driver/driver  ${BIN_DIR}/openf95
    INSTALL_EXEC_SUB ${AREA}/driver/kdriver ${BIN_DIR}/kopencc

    return 0
}

# Install front-end components
INSTALL_FE () {

    INSTALL_EXEC_SUB ${AREA}/gccfe/gfec  ${PHASEPATH}/gfec
    INSTALL_EXEC_SUB ${AREA}/g++fe/gfecc ${PHASEPATH}/gfecc
    INSTALL_EXEC_SUB ${AREA}/wgen/wgen ${PHASEPATH}/wgen
    INSTALL_EXEC_SUB ${GNUFE_AREA}/gcc/cc1 ${PHASEPATH}/cc1
    INSTALL_EXEC_SUB ${GNUFE_AREA}/gcc/cc1plus ${PHASEPATH}/cc1plus

    if [ -f ${AREA}/crayf90/sgi/mfef95 ] ; then 
      INSTALL_EXEC_SUB ${AREA}/crayf90/sgi/mfef95   ${PHASEPATH}/mfef95
      INSTALL_EXEC_SUB ${AREA}/crayf90/sgi/cf95.cat ${PHASEPATH}/cf95.cat
    fi
    return 0
}

# Install back-end components 
INSTALL_BE () {
    INSTALL_EXEC_SUB ${AREA}/be/be  ${PHASEPATH}/be
    INSTALL_EXEC_SUB ${AREA}/be/be.so ${PHASEPATH}/be.so

    return 0
}

# Install IPA-related components
INSTALL_IPA () {

    INSTALL_EXEC_SUB ${AREA}/ipa/ipa.so ${PHASEPATH}/ipa.so
    INSTALL_EXEC_SUB ${AREA}/ipl/ipl.so ${PHASEPATH}/ipl.so

    INSTALL_EXEC_SUB ${LD_NEW_DIR}/ld-new  ${PHASEPATH}/ipa_link

    ln -sf ${PHASEPATH}/be ${PHASEPATH}/ipl 

    return 0
}

# Install CG-related components
INSTALL_CG () {
    INSTALL_EXEC_SUB ${AREA}/cg/cg.so                ${PHASEPATH}/cg.so
    if [ "$TARG_HOST" = "ia64" ]; then
        # orc_ict.so and orc_intel.so is only valid on ia64
        INSTALL_EXEC_SUB ${AREA}/orc_ict/orc_ict.so      ${PHASEPATH}/orc_ict.so
        INSTALL_EXEC_SUB ${AREA}/orc_intel/orc_intel.so  ${PHASEPATH}/orc_intel.so
    fi
    return 0
}

INSTALL_WHIRL_STUFF () {

    INSTALL_EXEC_SUB  ${AREA}/whirl2c/whirl2c    ${PHASEPATH}/whirl2c
    INSTALL_EXEC_SUB  ${AREA}/whirl2f/whirl2f    ${PHASEPATH}/whirl2f
    INSTALL_EXEC_SUB  ${AREA}/whirl2c/whirl2c.so ${PHASEPATH}/whirl2c.so
    INSTALL_EXEC_SUB  ${AREA}/whirl2f/whirl2f.so ${PHASEPATH}/whirl2f.so

    (cd ${PHASEPATH}; ln -sf ${PHASEPATH}/be ${PHASEPATH}/whirl2c_be) 
    (cd ${PHASEPATH}; ln -sf ${PHASEPATH}/be ${PHASEPATH}/whirl2f_be) 

    INSTALL_EXEC_SUB  ${AREA}/ir_tools/ir_b2a    ${BIN_DIR}/ir_b2a
    INSTALL_EXEC_SUB  ${AREA}/libspin/gspin      ${BIN_DIR}/gspin

    return 0
}



# Install those archieves that are deemed as part of compiler, so 
# we put them where the orcc-phases reside.
INSTALL_PHASE_SPECIFIC_ARCHIVES () {

    if [ "$TARG_HOST" = "ia64" ] ; then
        # These stuffs are only valid on ia64
        # f90 related archieves 
        INSTALL_DATA_SUB ${AREA}/temp_f90libs/lib.cat  ${PHASEPATH}/lib.cat
        INSTALL_DATA_SUB ${AREA}/temp_f90libs/lib.exp  ${PHASEPATH}/lib.exp

        # instrument archieves.
        d="osprey/targ${TARG_HOST}/"
        INSTALL_DATA_SUB $d/libcginstr/libcginstr.a  ${PHASEPATH}/libcginstr.a  
        INSTALL_DATA_SUB $d/libinstr/libinstr.a      ${PHASEPATH}/libinstr.a 
        INSTALL_DATA_SUB $d/libepilog/libepilog.a    ${PHASEPATH}/libepilog.a


        #  SGI implementation for turning on FLUSH to ZERO
        INSTALL_DATA_SUB $d/init/ftz.o     ${PHASEPATH}/ftz.o
    fi

    # libgcc.a, libstdc++.a and libstdc++.so are deemed as "GNU link" specific archieves
    # is it necessary?
    for i in libgcc.a libstdc++.a libstdc++.so; do 
        F=`gcc --print-file-name $i`
        [ ! -z "$F" ] && [ -e $F ] && INSTALL_DATA_SUB $F ${PHASEPATH}/$i
    done

    return 0
}

# Install the general propose libraries, libfortran.a, libffio.a, libmsgi.a, libmv.a and libm.a   
INSTALL_GENERAL_PURPOSE_NATIVE_ARCHIVES () {

    if [ "$TARG_HOST" = "ia64" ] ; then
        LIBAREA="osprey/targ${TARG_HOST}/"
        INSTALL_DATA_SUB ${LIBAREA}/libfortran/libfortran.a ${PHASEPATH}/libfortran.a 
        INSTALL_DATA_SUB ${LIBAREA}/libu/libffio.a          ${PHASEPATH}/libffio.a
        INSTALL_DATA_SUB ${LIBAREA}/libmsgi/libmsgi.a       ${PHASEPATH}/libmsgi.a
        INSTALL_DATA_SUB ${LIBAREA}/libmv/libmv.a           ${PHASEPATH}/libmv.a
        INSTALL_DATA_SUB ${PREBUILT_LIB}/${TARG_HOST}-${TARG_OS}/gnu/libm.a ${PHASEPATH}/libm.a
	INSTALL_DATA_SUB ${AREA}/libopenmp/libopenmp.a      ${PHASEPATH}/libopenmp.a
    else
        LIBAREA=osprey/targx8664_builtonia32
        LIB32AREA=osprey/targia32_builtonia32
        # 64bit libraries
        INSTALL_DATA_SUB ${LIBAREA}/libfortran/libfortran.a ${PHASEPATH}/libfortran.a
        INSTALL_DATA_SUB ${LIBAREA}/libu/libffio.a          ${PHASEPATH}/libffio.a
        INSTALL_DATA_SUB ${LIBAREA}/libm/libmsgi.a       ${PHASEPATH}/libmsgi.a
        INSTALL_DATA_SUB ${LIBAREA}/libmv/libmv.a           ${PHASEPATH}/libmv.a
	INSTALL_DATA_SUB ${AREA}/libopenmp/libopenmp.a      ${PHASEPATH}/libopenmp.a
        # 32bit libraries
        INSTALL_DATA_SUB ${LIB32AREA}/libfortran/libfortran.a ${PHASEPATH}/32/libfortran.a
        INSTALL_DATA_SUB ${LIB32AREA}/libu/libffio.a          ${PHASEPATH}/32/libffio.a
        INSTALL_DATA_SUB ${LIB32AREA}/libm/libmsgi.a       ${PHASEPATH}/32/libmsgi.a
        INSTALL_DATA_SUB ${LIB32AREA}/libmv/libmv.a           ${PHASEPATH}/32/libmv.a
        INSTALL_DATA_SUB ${LIB32AREA}/libopenmp/libopenmp.a           ${PHASEPATH}/32/libopenmp.a
    fi 
    #install .h files
    INSTALL_DATA_SUB ./osprey/include/omp/omp.h     ${INC_DIR}/omp.h
    INSTALL_DATA_SUB ./osprey/include/whirl2c.h     ${INC_DIR}/whirl2c.h
    INSTALL_DATA_SUB ./osprey/include/whirl2f.h     ${INC_DIR}/whirl2f.h
    INSTALL_DATA_SUB ./osprey/f90modules/OMP_LIB_KINDS.mod  ${ROOT}/lib/f90modules/OMP_LIB_KINDS.mod 
    INSTALL_DATA_SUB ./osprey/f90modules/OMP_LIB.mod        ${ROOT}/lib/f90modules/OMP_LIB.mod


    return 0
}

INSTALL_PREBUILD_GNU_NATIVE_CRT_STARTUP () {

    for i in crtbegin.o crtend.o crtbeginS.o crtendS.o crtbeginT.o crtendT.o; do 
      f=`gcc --print-file-name=$i`
      [ "x$f" != "x" ] && [ -e $f ] && 
        INSTALL_DATA_SUB $f ${PHASEPATH}/$i
    done
    return 0
}


INSTALL_PREBUILD_OPEN64_NATIVE_LIB () {

    [ ! -d ${PREBUILT_LIB}/${PREBUILD_INTERPOS}/open64 ] && return 0

    for i in ${PREBUILT_LIB}/${PREBUILD_INTERPOS}/open64/* ; do 

        x=`basename $i`
        [ "$x" = "CVS" ] && continue;
        [ "$x" = ".svn" ] && continue;

        [ "$x" = "libinstr.a" ] &&
            INSTALL_DATA_SUB $i ${PHASEPATH}/$x && continue;
        [ "$x" = "libcginstr.a" ] &&
            INSTALL_DATA_SUB $i ${PHASEPATH}/$x && continue;

        INSTALL_DATA_SUB $i ${NATIVE_LIB_DIR}/`basename $i`
    done

    # install the 32bit prebuild libraries for x8664
    [ "$TARG_HOST" != "x8664" ] && return 0
    [ ! -d ${PREBUILT_LIB}/${PREBUILD_INTERPOS}/open64/32 ] && return 0
    for i in ${PREBUILT_LIB}/${PREBUILD_INTERPOS}/open64/32/* ; do

        x=`basename $i`
        [ "$x" = "CVS" ] && continue;
        [ "$x" = ".svn" ] && continue;

        INSTALL_DATA_SUB $i ${NATIVE_LIB_DIR}/32/$x
    done

    return 0
}

   # Install GNU glic-devel package. this is perform only for cross compilation. 
   # On native environment, we requires the end user install glibc-devel before 
   # hand.
INSTALL_PREBUILD_GLIBC_NATIVE_LIB () {

    [ ! -d ${PREBUILT_LIB}/${PREBUILD_INTERPOS}/gnu ] && return 0

    for i in ${PREBUILT_LIB}/${PREBUILD_INTERPOS}/gnu/* ; do 
        x=`basename $i`
        [ "$x" = "CVS" ] && continue;
        [ "$x" = ".svn" ] && continue;
        [ "$x" = "libstdc++.a" ] && continue; 
        [ "$x" = "libgcc.a"    ] && continue;
        INSTALL_EXEC_SUB $i ${NATIVE_LIB_DIR}/`basename $i`
    done  
    
    return 0
}

INSTALL_PREBUILD_PHASE () {

    # Some prebuild
    for i in ${PREBUILT_BIN}/${PREBUILD_INTERPOS}/phase/* ; do 
	[ ! -e $i ] && continue;
        [ "`basename $i`" = "CVS" ] && continue
        [ "`basename $i`" = ".svn" ] && continue
        INSTALL_EXEC_SUB $i ${PHASEPATH}/`basename $i`
    done

    return 0
}

INSTALL_CROSS_UTIL () {

    [ ! -d ${PREBUILT_BIN}/${PREBUILD_INTERPOS}/util ] && return 0

    for i in ${PREBUILT_BIN}/${PREBUILD_INTERPOS}/util/* ; do 
	[ "`basename $i`" = "CVS" ] && continue
        [ "`basename $i`" = ".svn" ] && continue
    	INSTALL_EXEC_SUB $i ${BIN_DIR}/`basename $i`
    done

    return 0
}

INSTALL_NATIVE_HEADER () {

    #INSTALL_DATA_SUB osprey/include/nue/stdarg.h  ${PHASEPATH}/include/stdarg.h
    #INSTALL_DATA_SUB osprey/include/nue/va-ia64.h  ${PHASEPATH}/include/va-ia64.h 
    #cp -f -a osprey/include ${PHASEPATH}/ 
    INSTALL_DATA_SUB ${ROOT}/include/whirl2c.h  ${ROOT}/include/${VERSION}/whirl2c.h
    INSTALL_DATA_SUB ${ROOT}/include/whirl2f.h  ${ROOT}/include/${VERSION}/whirl2f.h

    INSTALL_DATA_SUB ${AREA}/include/dwarf.h  ${ROOT}/include/${VERSION}/dwarf.h
    INSTALL_DATA_SUB ${AREA}/include/libdwarf.h  ${ROOT}/include/${VERSION}/libdwarf.h

    INSTALL_DATA_SUB ${AREA}/include/libelf/libelf.h  ${ROOT}/include/${VERSION}/libelf/libelf.h
    INSTALL_DATA_SUB ${AREA}/include/libelf/sys_elf.h  ${ROOT}/include/${VERSION}/libelf/sys_elf.h

    INSTALL_DATA_SUB ${AREA}/include/omp/omp.h  ${ROOT}/include/${VERSION}/omp.h

    return 0
}

INSTALL_MAN_PAGE () {

    d1=osprey/man/linux/man1
    d2=$ROOT/usr/man/man1

    INSTALL_DATA_SUB $d1/sgicc.1 $d2 
    INSTALL_DATA_SUB $d1/sgif90.1 $d2

    ln -sf $d2/sgicc.1  $d2/sgiCC.1

    return 0
}

INSTALL_MISC () {
    INSTALL_EXEC_SUB ${AREA}/wopt/wopt.so         ${PHASEPATH}/wopt.so
    INSTALL_EXEC_SUB ${AREA}/lw_inline/lw_inline  ${PHASEPATH}/inline
    INSTALL_EXEC_SUB ${AREA}/lno/lno.so           ${PHASEPATH}/lno.so

    if [ "$TARG_HOST" = "ia64" ]; then
        INSTALL_EXEC_SUB ${AREA}/targ_info/itanium.so ${PHASEPATH}/itanium.so
        INSTALL_EXEC_SUB ${AREA}/targ_info/itanium2.so ${PHASEPATH}/itanium2.so
    fi

    if [ "$TARG_HOST" = "x8664" ]; then
        INSTALL_EXEC_SUB ${AREA}/targ_info/opteron.so ${PHASEPATH}/opteron.so
        INSTALL_EXEC_SUB ${AREA}/targ_info/em64t.so ${PHASEPATH}/em64t.so
    fi
#    if [ ! -z "$ROOT" ] ; then
#        for i in gcc f77 as ld g++ gas as ; do
#            x=`which $i 2>/dev/null`
#            [ ! -z "$x" ] && ln -s $x $BIN_DIR/`basename $x` 2>/dev/null
#        done
#    fi

    # install some scripts
    [ ! -d ${PREBUILT_BIN}/misc ] && return 0
    for i in ${PREBUILT_BIN}/misc/* ; do 
        [ -f "$i" ] && INSTALL_EXEC_SUB ${i} ${BIN_DIR}/`basename $i`
    done

    return 0
}

cd `dirname $0`

[ ! -d ${BIN_DIR} ] && mkdir -p ${BIN_DIR}
[ ! -d ${NATIVE_LIB_DIR} ] && mkdir -p ${NATIVE_LIB_DIR}
if [ "$TARG_HOST" = "x8664" -a ! -d "${NATIVE_LIB_DIR}/32" ]; then
    mkdir -p ${NATIVE_LIB_DIR}/32
fi

INSTALL_DRIVER 
INSTALL_FE 
INSTALL_BE 
INSTALL_IPA 
INSTALL_CG 
INSTALL_WHIRL_STUFF 
INSTALL_MISC

#cat << _EOF_
# ------------------------------------------------------------------------
# NOTE: Following archives may not present. these archives are built on 
#   Native or NUE platform (by 'make library'), but do not
#   worry, prebuild verion of them are provided.
#  
#   {libcginstr.a libinstr.a ftz.o libfortran.a libffio.a
#                 libmsgi.a libmv.a}
#  
#   Normally, you need not to build these archives.
# ------------------------------------------------------------------------
#_EOF_

# Install archieves 
INSTALL_PHASE_SPECIFIC_ARCHIVES 
[ "$INSTALL_TYPE" = "ia64-cross" ] && INSTALL_PREBUILD_GLIBC_NATIVE_LIB 
[ "$INSTALL_TYPE" = "ia64-cross" ] && INSTALL_NATIVE_HEADER 
INSTALL_GENERAL_PURPOSE_NATIVE_ARCHIVES
INSTALL_PREBUILD_OPEN64_NATIVE_LIB 
INSTALL_PREBUILD_GNU_NATIVE_CRT_STARTUP 
[ "$INSTALL_TYPE" = "ia64-cross" ] && INSTALL_CROSS_UTIL
INSTALL_PREBUILD_PHASE 

exit 0

