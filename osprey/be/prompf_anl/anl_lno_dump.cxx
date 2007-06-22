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


#include <elf.h>                    /* for wn.h */
#include "wn.h"                     /* for WN */
#include "defs.h"		    /* for pu_info.h */ 
#include "config.h"                 /* for LNO_Path, etc. */
#include "pu_info.h"		    /* for PU_Info */ 
#include "w2c_driver.h"             /* for W2C_Process_Command_Line, etc. */
#include "w2f_driver.h"             /* for W2F_Process_Command_Line, etc. */
#include "anl_driver.h"             /* for Anl_Process_Command_Line, etc. */

extern WN_MAP Prompf_Id_Map; /* Maps WN constructs to unique identifiers */

#pragma weak W2C_Should_Emit_Nested_PUs
#pragma weak W2C_Outfile_Translate_Pu
#pragma weak W2F_Should_Emit_Nested_PUs
#pragma weak W2F_Outfile_Translate_Pu


extern void Prompf_Emit_Whirl_to_Source(PU_Info* current_pu,
                                        WN* func_nd)
{
  ST_IDX   st = PU_Info_proc_sym(current_pu);

  BOOL nested = ST_level(&St_Table[st]) > 2;

  if (Anl_Needs_Whirl2c() && 
      (W2C_Should_Emit_Nested_PUs() || !nested))
    W2C_Outfile_Translate_Pu(func_nd, TRUE);
  else if (Anl_Needs_Whirl2f() &&  
	   (W2F_Should_Emit_Nested_PUs() || !nested))
    W2F_Outfile_Translate_Pu(func_nd);
}


