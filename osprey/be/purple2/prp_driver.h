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

#ifndef prp_driver_INCLUDED
#define prp_driver_INCLUDED

#if defined(BUILD_OS_DARWIN)
/* Linux assumes weak declaration followed by definition and reference will
 * create a weak symbol which won't cause an error at link time; Mach-O
 * creates an undefined symbol which causes link to fail. */
#define Prp_Process_Command_Line(a, b, c, d) \
  assert(!"Prp_Process_Command_Line")
#define Prp_Needs_Whirl2c() 0
#define Prp_Needs_Whirl2f() 0
#define Prp_Init() assert(!"Prp_Init")
#define Prp_Instrument_And_EmitSrc(a) assert(!"Prp_Instrument_And_EmitSrc")
#define Prp_Fini() assert(!"Prp_Fini")
#else /* defined(BUILD_OS_DARWIN) */

#ifdef __cplusplus
extern "C" {
#endif
/* ==============================================================
 * ==============================================================
 *
 * Module: prp_driver.h
 * $Revision: 1.1 $
 * $Date: 2005/07/27 02:13:47 $
 * $Author: kevinlo $
 * $Source: /depot/CVSROOT/javi/src/sw/cmplr/be/purple2/prp_driver.h,v $
 *
 * Description:
 *
 *   Defines utilities exported by purple_instr.so
 *
 * ==============================================================
 * ==============================================================
 */

void Prp_Process_Command_Line (INT phase_argc, char *phase_argv[],
			       INT argc, char *argv[]);
BOOL Prp_Needs_Whirl2c(void);
BOOL Prp_Needs_Whirl2f(void);
void Prp_Init(void);
void Prp_Instrument_And_EmitSrc(WN *pu);
void Prp_Fini(void);

#ifdef __cplusplus
}
#endif
#endif /* defined(BUILD_OS_DARWIN) */

#endif /* prp_driver_INCLUDED */
