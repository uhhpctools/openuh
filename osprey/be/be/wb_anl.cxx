/*
 * Copyright 2004 PathScale, Inc.  All Rights Reserved.
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


#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <sys/types.h>
#include <elf.h>
#include <ctype.h>
#include "wn.h"
#include "wn_map.h"
#include "wn_util.h"
#include <stdio.h>
#include "opt_du.h"
#include "opt_alias_mgr.h"
#include "dep_graph.h"
#include "prompf.h"
#include "ir_reader.h"
#include "wb_util.h"
#include "wb_buffer.h"
#include "wb_carray.h"
#include "wb_browser.h"
#include "wb.h"
#include "wb_anl.h"

WB_BROWSER wb_anl; 

extern void WB_ANL_Initialize(WN* wn_global,
			      WN_MAP prompf_id_map) 
{ 
  WB_Set_Phase(WBP_PROMPF_SA); 
  WB_Initialize(&wb_anl, wn_global, &Get_Current_PU(), NULL, NULL, 
    prompf_id_map); 
} 

extern void WB_ANL_Terminate()
{ 
  WB_Set_Phase(WBP_NONE); 
  WB_Terminate(&wb_anl); 
} 

extern void s_anl_debug(char init_buffer[])
{ 
  wb_anl.Sdebug(init_buffer); 
} 

