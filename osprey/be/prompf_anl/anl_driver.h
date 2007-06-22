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


#ifndef anl_driver_INCLUDED
#define anl_driver_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
/* ==============================================================
 * ==============================================================
 *
 * Module: anl_driver.h
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/prompf_anl/anl_driver.h,v $
 *
 * Description:
 *
 *   Defines utilities exported by prompf_anl.so.  The map returned
 *   by Anl_Static_Analysis() will be used later by whirl2[cf] and
 *   by LNO to emit information about transformed constructs.
 *
 * ==============================================================
 * ==============================================================
 */

void        Anl_Process_Command_Line (INT phase_argc, char *phase_argv[],
				      INT argc, char *argv[]);
BOOL        Anl_Needs_Whirl2c(void);
BOOL        Anl_Needs_Whirl2f(void);
void        Anl_Init(void);
WN_MAP	    Anl_Init_Map(MEM_POOL *id_map_pool); 
void        Anl_Static_Analysis(WN *pu, WN_MAP id_map);
INT64       Get_Next_Construct_Id(void);
INT64       New_Construct_Id(void);
const char *Anl_File_Path(void);
void        Anl_Fini(void);
void        Anl_Cleanup(void);

#ifdef __cplusplus
}
#endif
#endif /* anl_driver_INCLUDED */
