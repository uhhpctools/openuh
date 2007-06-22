/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


# if 0  

/* USMID:  "\n@(#)5.0_pl/headers/defines.h	5.17	10/20/99 17:17:46\n" */

/******************************************************************************/
/******************************************************************************/
/*									      */
/* This file is used to convert commandline definitions into internal         */
/* compiler definitions.  (Note:  Also check cmd_line.h and type.h)           */
/*									      */
/* The possible commandline defines are :                                     */
/*									      */
/* _PVP_PVP                 Host CRAY to CRAY                                 */
/* _PVP_MPP                 Host CRAY - target MPP                            */
/* _MPP_MPP                 Host MPP - target MPP                             */
/* _SGI_SGI                 Host SGI - target SGI                             */
/* _SGI_SV2                 Host SGI - target SV2                             */
/* _SOLARIS_SOLARIS         Host solaris - target solaris                     */
/* _LINUX_LINUX             Host linux - target_linux  (assume intel for now) */
/*                          This will change to a more meaningful name once   */
/*                          we figure out what that will be.                  */
/*									      */
/* _EXPR_EVAL    	    Can be specified with one of the above.  This     */
/*               	    creates a frontend that can be used as the        */
/*               	    expression evaluator for the debugger.            */
/*									      */
/* _DEBUG                   Builds a DEBUG version of the frontend.	      */
/*									      */
/*									      */
/******************************************************************************/
/*									      */
/* Internal defines are:                                                      */
/*									      */
/*   Choose 1:								      */
/*	_ENABLE_FEI             Activates functional interface        	      */
/*									      */
/*   Choose 1:								      */
/*	_HOST32                 Host bit size is 32 bits		      */
/*	_HOST64                 Host bit size is 64 bits		      */
/*									      */
/*   Choose 1:								      */
/*	_TARGET32               Target bit size is 32 bits.		      */
/*	_TARGET64               Target bit size is 64 bits.		      */
/*									      */
/*									      */
/*   When adding a new set of host and target macros, don't forget to go      */
/*   into ntr_const_tbl and use them to determine when numeric constant       */
/*   searches can be optimized.  See the comments in ntr_const_tbl.           */
/*									      */
/*   Choose 1:								      */
/*	_HOST_OS_MAX            Host operating system is MAX.		      */
/*	_HOST_OS_SOLARIS        Host operating system is SOLARIS.	      */
/*	_HOST_OS_UNICOS         Host operating system is UNICOS.	      */
/*	_HOST_OS_IRIX           Host operating system is IRIX.		      */
/*	_HOST_OS_LINUX          Host operating system is LINUX.		      */
/*									      */
/*   Choose 1:                                           	 	      */
/*	_TARGET_OS_MAX          Target operating system is MAX		      */
/*	_TARGET_OS_SOLARIS      Target operating system is SOLARIS	      */
/*	_TARGET_OS_UNICOS       Target operating system is UNICOS	      */
/*	_TARGET_OS_IRIX         Target operating system is IRIX.	      */
/*	_TARGET_OS_LINUX        Target operating system is LINUX.	      */
/*									      */
/*   Choose 1: 						 		      */
/*	_TARGET_IEEE            IEEE					      */
/*									      */
/*   Choose 1: 						 		      */
/*	_TARGET_SV2             SV2					      */
/*									      */
/*   Choose 1: 						 		      */
/*	_TARGET_LITTLE_ENDIAN	LITTLE ENDIAN				      */
/*									      */
/*   Choose 1: 						 		      */
/*	_HOST_LITTLE_ENDIAN	LITTLE ENDIAN				      */
/*									      */
/*   Choose 1:								      */
/*	_TARGET_BYTE_ADDRESS    Addressing is done by bytes on the target.    */
/*	_TARGET_WORD_ADDRESS    Addressing is done by words on the target.    */
/*									      */
/*   Choose 1:								      */
/*	_HEAP_REQUEST_IN_BYTES  Heap requests are in bytes.                   */
/*	_HEAP_REQUEST_IN_WORDS  Heap requests are in words.                   */
/*									      */
/*   Choose 1:								      */
/*	_MODULE_TO_DOT_o        If file being compiled is called name.f, then */
/*			        the module information tables will be sent to */
/*			        the name.o (binary) file via the interface.   */
/*			        A tmp file name is sent through the interface */
/*			        The backend is required to put it where it    */
/*			        wants it in the .o and then delete the file.  */
/*			        A mechanism has to be implemented to find     */
/*			        the module files again.  This frontend        */
/*			        supports Cray loader tables and SUN ELF files.*/
/*			        The current naming scheme for the tmp files   */
/*			        is .filename.MODULENAME.m                     */
/*                              If -dm is specified, this is the default.     */
/*                              -em (module to dot mod) overrides this.       */
/*	_MODULE_TO_DOT_M        If file being compiled is called name.f, then */
/*			        the module information tables will be sent to */
/*			        name.M directly using fprintf statements.     */
/*			        When searching for modules, the compiler will */
/*			        look for .M files.  If a user specified       */
/*			        -p name.o, the file -p name.M will be         */
/*			        when resolving USE statements.                */
/*                              If -dm is specified, this is the default.     */
/*                              -em (module to dot mod) overrides this.       */
/*									      */
/*	                 MODULE NOTE: Check MODULE_USE_SYSTEM_FILE and        */
/*	                        MODULE_USE_SYSTEM_PATH_VAR for information    */
/*	                        on setting a system file for the compiler to  */
/*	                        search when resolving USE statements.         */
/*									      */
/*   Misc:								      */
/*	_DEBUG                  Builds a DEBUG version of the frontend.	      */
/*	                        Set on the build commandline.                 */
/*	_ALIGN_REAL16_TO_16_BYTES					      */
/*				Align real(kind=16) and compilex(kind=16)     */
/*				to a 16 byte boundary.                        */
/*	_ALLOCATE_IS_CALL       ALLOCATE stmt stays as user library call      */
/*				otherwise it is an operator.                  */
/*	_ALLOW_DATA_INIT_OF_COMMON   Issue an ANSI message if a common block  */
/*	                        is initialized outside of a blockdata.  If    */
/*	                        this is not specified, a warning is issued.   */
/*      _ALTERNATIVE_INTERFACE_FOR_POINTEES                                   */
/*                              A different interface to describe pointees    */
/*                              to the backend.                               */
/*	_ARITH_H                Needed if arith.h is used.         	      */
/*	_ARITH_INPUT_CONV       Use arith to do input conversion	      */
/*                              otherwise use libc input conversion.          */
/*	_ASSIGN_OFFSETS_TO_HOSTED_STACK   True means assign offets to host    */
/*                              associated stack variables, move hosted equiv */
/*                              groups into the host associated stack and     */
/*                              generate a temp at the end of the hosted stack*/
/*                              for each child that uses the hosted stack.    */
/*                              False means we assign no offsets to this block*/
/*                              we generate no temp and hosted stack equiv    */
/*                              items are put in their own stack block, but   */
/*                              marked as host associated.                    */
/*	_CHAR_IS_ALIGN_8	We attach an alignment enum to all variables  */
/*	                        which is used on some platforms.  If the type */
/*	                        is character, the alignment is set to Align_8 */
/*	                        If this is not set, alignment is Align_Bit    */
/*	_CHAR_LEN_IN_BYTES      Character length in in bytes.  If this is not */
/*			        defined, character length is in bits.         */
/*	_CHECK_MAX_MEMORY       Check for objects bigger than memory size.    */
/*	_D_LINES_SUPPORTED	'd' in column one is either comment or blank  */
/*                              depending on command line option.             */
/*	_DOPE_VECTOR_32_OR_64	Use a switchable dope vector based on the     */
/*                              -s pointer8 commandline option.               */
/*	_ERROR_DUPLICATE_GLOBALS   An error message will be issued if their   */
/*	                        are duplicate global or common entries found  */
/*	                        during compilation.  If this is not defined   */
/*	                        caution level messages will be issued.        */
/*	_EXTENDED_CRI_CHAR_POINTER   turns on code to allow array character   */
/*                              pointees and other char pointer changes.      */
/*	_F_MINUS_MINUS	        Used to allow F-- syntax. (processor          */
/*			        dimension syntax array(1:10)[2,3] )           */
/*	_FILE_IO_OPRS		If this is set, OPEN,CLOSE,INQUIRE,BACKSPACE, */
/*				BUFFER IO, ENDFILE, and REWIND stmts leave the*/
/*				frontend as operators rather than calls.      */
/*	_FRONTEND_CONDITIONAL_COMP  This turns on the conditional             */
/*			        compilation support within the frontend.  It  */
/*				is not used at this time on any platforms.    */
/*				Gpp (or an equivalent preprocessor) is being  */
/*				used instead.                                 */
/*	_FRONTEND_INLINER	This enables the frontend inliner.            */
/*	_GEN_LOOPS_FOR_DV_WHOLE_DEF  Generate explicit loops for compiler     */
/*                              generated Dv_Def_Asg_Opr for arrays.          */
/*	_GETPMC_AVAILABLE	Library routine GETPMC is available for       */
/*	                        picking up machine characteristics.           */
/*      _HIGH_LEVEL_DO_LOOP_FORM   Leave the IR of DO loops (particularly     */
/*				iterative DO loops) in a high-level form (do  */
/*				not generate all the tests and branches       */
/*				normally required for an iterative DO loop).  */
/*      _HIGH_LEVEL_IF_FORM     Leave the IR of IF's in a high-level form.    */
/*				Do not generate all the tests and branches    */
/*				normally required for an IF.                  */
/*	_INIT_RELOC_BASE_OFFSET Translate Init_Reloc_Opr into base + offset.  */
/*	_INTEGER_1_AND_2	support 8 and 16 bit integer and logical.     */
/*	                        There is a commandline option that controls   */
/*	                        this underneath the define. Check ACCEPT in   */
/*	                        target.m for _ed_h.  This must be set TRUE    */
/*	                        and the cmdline option used or always set on. */
/*      _NAME_SUBSTITUTION_INLINING                                           */
/*                              This is a form of inlining that does complete */
/*                              name substitution for array mappings.         */
/*	_NEED_AT_SIGN_INTRINSICSThe system needs versions of intrinsics that  */
/*	                        are suffixed with an @.  This is necessary    */
/*	                        to support the multi-processing source to     */
/*	                        source pre-processor.                         */
/*	_NO_AT_SIGN_IN_NAMES    The GNU assembler will not accept @ in names. */
/*	                        When this is in effect, @'s will not be gen'd */
/*	                        in external names.                            */
/*	_NO_BINARY_OUTPUT       Generated code is an assembly listing coming  */
/* 				out of the backend.  This is necessary for    */
/* 				a functional compiler, so that the frontend   */
/* 				can tell the backend whether to do binary or  */
/* 				assembly output.   For error purposes the     */
/* 				frontend will act like binary output is       */
/* 				default.  We just need to switch to assembly  */
/* 				at the interface.                             */
/*	_NO_CRAY_CHARACTER_PTR  Cray character pointer is not allowed.  Issue */
/*	                        error if seen.                                */
/*	_NO_IO_ALTERNATE_RETURN Do not generate the alternate return branch   */
/*				operation for END=, ERR=, EOF= in io stmts.   */
/*				The labels are sent as arguments 11,12,13 of  */
/*				io fei_control_list().                        */
/*	_POINTEES_CAN_BE_STRUCT Allow CRI pointers to point to structures.    */
/*			        ie:  Pointees can be typed as structure.      */
/*      _QUAD_PRECISION         Quad precision ia accepted.                   */
/*	_SAVE_IO_STMT		Used to save the original I/O statement as a  */
/*				Fortran character literal.  Operators are     */
/*				inserted to start and end the expanded I/O    */
/*				with the character literal attached to the    */
/*				'start io' operator.                          */
/*	_SEPARATE_DEALLOCATES	Generate seperate calls to DEALLOCATE routines*/
/*	_SEPARATE_FUNCTION_RETURNS   If more than one entry in function,      */
/*                              there is a return opr for each entry,         */
/*                              otherwise there is only one return opr.       */
/*	_SEPARATE_NONCOMMON_EQUIV_GROUPS   If set, then static, hosted_static */
/*	                        and hosted_stack equivalence groups each get  */
/*	                        their own storage group.                      */
/*	_SINGLE_ALLOCS_FOR_AUTOMATIC    Each automatic gets its own allocate  */
/*	                        call, rather than one allocate for all autos. */
/*	_SM_UNIT_IS_ELEMENT	The unit for stride multipliers is in terms of*/
/*                              elements, rather than words, half words, bytes*/
/*	_SPLIT_STATIC_STORAGE_M only split static module storage into         */
/*				initialized vs uninitialized storage.         */
/*      _SPLIT_STATIC_STORAGE_2 split static storage into initialized vs      */
/*                              uninitialized storage.                        */
/*      _SPLIT_STATIC_STORAGE_3 split static storage into initialized vs      */
/*                              uninitialized storage.  Uninitialized is      */
/*                              further split into scalar vs aggregate.       */
/*	_STOP_IS_OPR		Do not lower STOP to a call.                  */
/*	_TARGET_DOUBLE_ALIGN    Double word types on will have their offsets  */
/*	                        aligned on double word boudaries              */
/*      _TARGET_HAS_FAST_INTEGER                                              */
/*                              Fast integer is available on this target.     */
/*                              (46 bit integer)                              */
/*	_TARGET_PACK_HALF_WORD_TYPES   On a 64 bit machine, 32 bit types will */
/*	                        be packed on 32 bit boundaries.  Complex and  */
/*	                        double precision will always be aligned on 64 */
/*	_TASK_COMMON_EXTENSION  Support the TASK COMMON extension.            */
/*	_THREE_CALL_IO	        Split io stmt completely into a 3 call model. */
/*	_TMP_GIVES_COMMON_LENGTH  Used for Cray backends only.  Currently     */
/*	                        they need a temp generated at the end of a    */
/*	                        common block so that Cray backends can        */
/*	                        determine the length of a common block when   */
/*	                        it is partially host associated or use        */
/*	                        associated.  For non-Cray use the correct     */
/*	                        block length is available when the block is   */
/*	                        described using the interfaces.               */
/*      _TRANSFORM_CHAR_SEQUENCE    transform character sequence derived      */
/*                              type references to overindexed substring      */
/*                              references of the first character component.  */
/*                              This is necessary for word addressable        */
/*                              machines and any compiler that uses pdgcs.    */
/*	_TWO_WORD_FCD           The Fortran Character Descriptor is 2 words.  */
/*	_USE_FOLD_DOT_f         Use the set of Fortran routines for compile   */
/*	                        time folding.                                 */
/*	_WARNING_FOR_NUMERIC_INPUT_ERROR				      */
/*				Make all numeric input conversion Errors      */
/*				come out as Warnings.                         */
/*									      */
/*									      */
/*									      */
/******************************************************************************/
/******************************************************************************/
# endif

# ifdef _PVP_PVP
#	define _ENABLE_FEI     			1
#	define _HOST64				1
#	define _TARGET64			1
#	define _HOST_OS_UNICOS			1
#	define _TARGET_OS_UNICOS		1
#	define _HEAP_REQUEST_IN_WORDS		1
#	define _TARGET_WORD_ADDRESS		1
#	define _MODULE_TO_DOT_o			1
#	define _ARITH_H				1
#	define _ARITH_INPUT_CONV		1
#	define _ALLOW_DATA_INIT_OF_COMMON	1
#	define _ASSIGN_OFFSETS_TO_HOSTED_STACK	1
#	define _CHECK_MAX_MEMORY		1
#	define _EXTENDED_CRI_CHAR_POINTER	1
#	define _F_MINUS_MINUS			1
#	define _FRONTEND_CONDITIONAL_COMP	1
#	define _GETPMC_AVAILABLE		1
#	define _NEED_AT_SIGN_INTRINSICS		1
#	define _POINTEES_CAN_BE_STRUCT		1
#	define _SPLIT_STATIC_STORAGE_3		1
#	define _TARGET_HAS_FAST_INTEGER		1
#	define _TASK_COMMON_EXTENSION		1
#	define _TRANSFORM_CHAR_SEQUENCE		1
#	define _TMP_GIVES_COMMON_LENGTH		1
#	define _FRONTEND_INLINER		1
#	define _D_LINES_SUPPORTED		1
# endif

# ifdef _MPP_MPP
#	define _ENABLE_FEI     			1
#	define _HOST64				1
#	define _TARGET64			1
#	define _HOST_OS_MAX			1
#	define _TARGET_OS_MAX			1
#	define _TARGET_IEEE			1
#	define _TARGET_BYTE_ADDRESS		1
#	define _HEAP_REQUEST_IN_WORDS		1
#	define _MODULE_TO_DOT_o			1
#	define _ARITH_H				1
#	define _ARITH_INPUT_CONV		1
#	define _ALLOW_DATA_INIT_OF_COMMON	1
#	define _ASSIGN_OFFSETS_TO_HOSTED_STACK	1
#	define _CHECK_MAX_MEMORY		1
#	define _ERROR_DUPLICATE_GLOBALS		1
#	define _EXTENDED_CRI_CHAR_POINTER	1
#	define _F_MINUS_MINUS			1
#	define _FRONTEND_CONDITIONAL_COMP	1
#	define _GETPMC_AVAILABLE		1
#	define _POINTEES_CAN_BE_STRUCT		1
#	define _SPLIT_STATIC_STORAGE_3		1
#	define _TARGET_PACK_HALF_WORD_TYPES	1
#	define _TMP_GIVES_COMMON_LENGTH		1
#	define _TRANSFORM_CHAR_SEQUENCE		1
#	define _TWO_WORD_FCD			1
#	define _FRONTEND_INLINER		1
#	define _INIT_RELOC_BASE_OFFSET		1
#	define _D_LINES_SUPPORTED		1
# endif

# ifdef _SGI_SV2
#       define _ENABLE_FEI                      1
#       define _HOST32                          1
#       define _TARGET32                        1
#       define _HOST_OS_IRIX                    1
#       define _TARGET_OS_UNICOS                1
#       define _TARGET_IEEE                     1
#       define _TARGET_SV2                      1
#	define _DOPE_VECTOR_32_OR_64		1
#       define _TARGET_BYTE_ADDRESS             1
#       define _HEAP_REQUEST_IN_BYTES           1
#	define _MODULE_TO_DOT_o			1
#       define _ARITH_H                         1
#       define _ARITH_INPUT_CONV                1
#	define _ALLOW_DATA_INIT_OF_COMMON	1
#	define _ASSIGN_OFFSETS_TO_HOSTED_STACK	1
#       define _CHAR_LEN_IN_BYTES               1
#       define _CHECK_MAX_MEMORY                1
#	define _ERROR_DUPLICATE_GLOBALS		1
#       define _F_MINUS_MINUS                   1
#       define _FRONTEND_CONDITIONAL_COMP       1
#       define _INTEGER_1_AND_2                 1
#	define _POINTEES_CAN_BE_STRUCT		1
#       define _QUAD_PRECISION                  1
#       define _SPLIT_STATIC_STORAGE_M          1
#       define _TARGET_DOUBLE_ALIGN             1
#	define _TMP_GIVES_COMMON_LENGTH		1
#       define _TWO_WORD_FCD                    1
#	define _FRONTEND_INLINER		1
#	define _SM_UNIT_IS_ELEMENT		1
#	define _D_LINES_SUPPORTED		1
# endif

# ifdef _SGI_SGI
#       define _ENABLE_FEI                      1
#	define _HOST32				1
#	define _TARGET32			1
#	define _HOST_OS_IRIX			1
#	define _TARGET_OS_IRIX			1
#	define _TARGET_IEEE			1
#	define _DOPE_VECTOR_32_OR_64		1
#	define _TARGET_BYTE_ADDRESS		1
#	define _HEAP_REQUEST_IN_BYTES		1
#	define _ARITH_H				1
#	define _ARITH_INPUT_CONV		1
#	define _ALIGN_REAL16_TO_16_BYTES	1
#	define _CHAR_IS_ALIGN_8			1
#	define _CHAR_LEN_IN_BYTES		1
#	define _CHECK_MAX_MEMORY		1
#	define _ERROR_DUPLICATE_GLOBALS		1
#	define _F_MINUS_MINUS			1
#	define _FILE_IO_OPRS			1
#	define _FRONTEND_CONDITIONAL_COMP	1
#	define _GEN_LOOPS_FOR_DV_WHOLE_DEF	1
#	define _HIGH_LEVEL_DO_LOOP_FORM		1
#	define _HIGH_LEVEL_IF_FORM		1
#	define _INTEGER_1_AND_2			1
#	define _NAME_SUBSTITUTION_INLINING	1
#	define _NO_CRAY_CHARACTER_PTR		1
#	define _NO_IO_ALTERNATE_RETURN		1
#	define _POINTEES_CAN_BE_STRUCT		1
#       define _QUAD_PRECISION                  1
#	define _SAVE_IO_STMT			1
#	define _SEPARATE_NONCOMMON_EQUIV_GROUPS	1
#	define _SPLIT_STATIC_STORAGE_M		1
#	define _SINGLE_ALLOCS_FOR_AUTOMATIC	1
#	define _TARGET_DOUBLE_ALIGN		1
#	define _TWO_WORD_FCD			1
#	define _WARNING_FOR_NUMERIC_INPUT_ERROR	1
#       define _ALTERNATIVE_INTERFACE_FOR_POINTEES 1
#	define _SEPARATE_DEALLOCATES		1
#	define _STOP_IS_OPR			1
#	define _TYPE_CODE_64_BIT		1
#	define _D_LINES_SUPPORTED		1
# endif

# ifdef _SOLARIS_SOLARIS
#	define _HOST32				1
#	define _TARGET32			1
#	define _HOST_OS_SOLARIS			1
#	define _TARGET_OS_SOLARIS		1
#	define _TARGET_IEEE			1
#	define _TARGET_BYTE_ADDRESS		1
#	define _HEAP_REQUEST_IN_BYTES		1
#	define _MODULE_TO_DOT_o			1
#	define _ARITH_H				1
#	define _ARITH_INPUT_CONV		1
#	define _ASSIGN_OFFSETS_TO_HOSTED_STACK	1
#	define _CHAR_LEN_IN_BYTES		1
#	define _CHECK_MAX_MEMORY		1
#	define _ERROR_DUPLICATE_GLOBALS		1
#	define _FRONTEND_CONDITIONAL_COMP	1
#	define _NO_BINARY_OUTPUT		1
#       define _QUAD_PRECISION                  1
#	define _SEPARATE_FUNCTION_RETURNS	1
#	define _SPLIT_STATIC_STORAGE_2		1
#	define _TARGET_DOUBLE_ALIGN		1
#	define _TASK_COMMON_EXTENSION		1
#	define _TMP_GIVES_COMMON_LENGTH		1
#	define _TRANSFORM_CHAR_SEQUENCE		1
#	define _TWO_WORD_FCD			1
#	define _FRONTEND_INLINER		1
#	define _INIT_RELOC_BASE_OFFSET		1
#	define _D_LINES_SUPPORTED		1
# endif

# ifdef _PVP_MPP
#	define _ENABLE_FEI     			1
#	define _HOST64				1
#	define _TARGET64			1
#	define _HOST_OS_UNICOS			1
#	define _TARGET_OS_MAX			1
#	define _TARGET_IEEE			1
#	define _TARGET_BYTE_ADDRESS		1
#	define _HEAP_REQUEST_IN_WORDS		1
#	define _MODULE_TO_DOT_o			1
#	define _ARITH_H				1
#	define _ARITH_INPUT_CONV		1
#	define _ALLOW_DATA_INIT_OF_COMMON	1
#	define _ASSIGN_OFFSETS_TO_HOSTED_STACK	1
#	define _CHECK_MAX_MEMORY		1
#	define _ERROR_DUPLICATE_GLOBALS		1
#	define _F_MINUS_MINUS			1
#	define _FRONTEND_CONDITIONAL_COMP	1
#	define _GETPMC_AVAILABLE		1
#	define _POINTEES_CAN_BE_STRUCT		1
#	define _SPLIT_STATIC_STORAGE_3		1
#	define _TARGET_PACK_HALF_WORD_TYPES	1
#	define _TRANSFORM_CHAR_SEQUENCE		1
#	define _TMP_GIVES_COMMON_LENGTH		1
#	define _TWO_WORD_FCD			1
#	define _FRONTEND_INLINER		1
#	define _INIT_RELOC_BASE_OFFSET		1
#	define _D_LINES_SUPPORTED		1
# endif




# ifdef _LINUX_LINUX

# if 0
/*******************************************************/
/*  These are necessary to compile on linux.   We're   */
/*  not sure what they are.                            */
/*******************************************************/
# endif

# if 0
/*      This is needed for a library include file */
# endif

#	define _LITTLE_ENDIAN			1
#       define _GNU_SOURCE                      1
#       define _NO_XOPEN4                       1
#	define _HOST_LITTLE_ENDIAN		1
#	define _TARGET_LITTLE_ENDIAN		1
#       define _ENABLE_FEI                      1
# ifdef _LP64
#	define _HOST64				1
#	define _TARGET64			1
#	define _WHIRL_HOST64_TARGET64		1
#	define _TARGET_PACK_HALF_WORD_TYPES	1
# else
#	define _HOST32				1
#	define _TARGET32			1
#	define _DOPE_VECTOR_32_OR_64		1
#	define _TARGET_DOUBLE_ALIGN		1
# endif /* _LP64 */
#	define _HOST_OS_LINUX			1
#	define _TARGET_OS_LINUX			1
/* 30Jan01[sos] commented out: #	define _HOST_OS_IRIX			1 */
/* 30Jan01[sos] commented out: #	define _TARGET_OS_IRIX			1 */
#	define _TARGET_IEEE			1
#	define _TARGET_BYTE_ADDRESS		1
#	define _HEAP_REQUEST_IN_BYTES		1
#	define _ARITH_H				1
#	define _ALIGN_REAL16_TO_16_BYTES	1
#	define _CHAR_IS_ALIGN_8			1
#	define _CHAR_LEN_IN_BYTES		1
#	define _CHECK_MAX_MEMORY		1
#	define _ERROR_DUPLICATE_GLOBALS		1
#	define _F_MINUS_MINUS			1
#	define _FILE_IO_OPRS			1
#	define _FRONTEND_CONDITIONAL_COMP	1
#	define _GEN_LOOPS_FOR_DV_WHOLE_DEF	1
#	define _HIGH_LEVEL_DO_LOOP_FORM		1
#	define _HIGH_LEVEL_IF_FORM		1
#	define _INTEGER_1_AND_2			1
#	define _NAME_SUBSTITUTION_INLINING	1
#	define _NO_AT_SIGN_IN_NAMES		1
/* Bug 2001 */
#       ifdef KEY
#       define _EXTENDED_CRI_CHAR_POINTER       1
#       else
#	define _NO_CRAY_CHARACTER_PTR		1
#       endif
#	define _NO_IO_ALTERNATE_RETURN		1
#	define _POINTEES_CAN_BE_STRUCT		1
#       define _QUAD_PRECISION                  1
#	define _SAVE_IO_STMT			1
#	define _SEPARATE_NONCOMMON_EQUIV_GROUPS	1
#	define _SPLIT_STATIC_STORAGE_M		1
#	define _SINGLE_ALLOCS_FOR_AUTOMATIC	1
#	define _TWO_WORD_FCD			1
#	define _USE_FOLD_DOT_f			1
#	define _WARNING_FOR_NUMERIC_INPUT_ERROR	1
#       define _ALTERNATIVE_INTERFACE_FOR_POINTEES 1
#	define _SEPARATE_DEALLOCATES		1
#	define _STOP_IS_OPR			1
#	define _TYPE_CODE_64_BIT		1
#	define _SEPARATE_FUNCTION_RETURNS	1
#	define _D_LINES_SUPPORTED		1
# endif
