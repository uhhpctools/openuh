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


#ifndef w2c_weak_INCLUDED
#define w2c_weak_INCLUDED

// See comments in be/com/weak.cxx

// Symbols exported from whirl2c.so 
// and used in be, be.so and lno.so
// (additional ones may be needed for prompf_anl.so)

#ifdef __linux__

extern void (*W2C_Cleanup_p)(void);
extern void (*W2C_Init_p)(void);
extern void (*W2C_Outfile_Fini_p)(BOOL emit_global_decls);
extern void (*W2C_Outfile_Init_p)(BOOL emit_global_decls);
extern void (*W2C_Outfile_Translate_Pu_p)(WN *pu, BOOL emit_global_decls);
extern void (*W2C_Pop_PU_p)(void);
extern void (*W2C_Process_Command_Line_p)(INT phase_argc, char * const
  phase_argv[], INT argc, char * const argv[]);
extern void (*W2C_Push_PU_p)(const WN *pu, WN *body_part_of_interest);
extern void (*W2C_Set_Frequency_Map_p)(WN_MAP frequency_map);
extern void (*W2C_Set_Prompf_Emission_p)(const WN_MAP *construct_map);
extern BOOL (*W2C_Should_Emit_Nested_PUs_p)(void);
extern void (*W2C_Translate_Istore_Lhs_p)(char *strbuf,
  UINT bufsize, const WN* lhs, mINT64 istore_ofst, TY_IDX istore_addr_ty,
  TYPE_ID istore_mtype);
extern void (*W2C_Translate_Wn_p)(FILE *outfile, const WN *wn);

#define W2C_Cleanup (*W2C_Cleanup_p)
#define W2C_Init (*W2C_Init_p)
#define W2C_Outfile_Fini (*W2C_Outfile_Fini_p)
#define W2C_Outfile_Init (*W2C_Outfile_Init_p)
#define W2C_Outfile_Translate_Pu (*W2C_Outfile_Translate_Pu_p)
#define W2C_Pop_PU (*W2C_Pop_PU_p)
#define W2C_Process_Command_Line (*W2C_Process_Command_Line_p)
#define W2C_Push_PU (*W2C_Push_PU_p)
#define W2C_Set_Prompf_Emission (*W2C_Set_Prompf_Emission_p)
#define W2C_Set_Frequency_Map (*W2C_Set_Frequency_Map_p)
#define W2C_Should_Emit_Nested_PUs (*W2C_Should_Emit_Nested_PUs_p)
#define W2C_Translate_Istore_Lhs (*W2C_Translate_Istore_Lhs_p)
#define W2C_Translate_Wn (*W2C_Translate_Wn_p)

#else // __linux__

#pragma weak W2C_Cleanup
#pragma weak W2C_Init
#pragma weak W2C_Outfile_Fini
#pragma weak W2C_Outfile_Init
#pragma weak W2C_Outfile_Translate_Pu
#pragma weak W2C_Pop_PU
#pragma weak W2C_Process_Command_Line
#pragma weak W2C_Push_PU
#pragma weak W2C_Set_Frequency_Map
#pragma weak W2C_Set_Prompf_Emission
#pragma weak W2C_Should_Emit_Nested_PUs
#pragma weak W2C_Translate_Istore_Lhs
#pragma weak W2C_Translate_Wn
#pragma weak W2C_Translate_Wn_Str

#endif // __linux__

#endif // w2c_weak_INCLUDED
