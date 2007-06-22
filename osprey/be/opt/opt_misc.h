//-*-c++-*-

// ====================================================================
// ====================================================================
//
// Copyright (C) 2007, University of Delaware, Hewlett-Packard Company,
// All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it would be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Further, this software is distributed without any warranty that it
// is free of the rightful claim of any third person regarding
// infringement  or the like.  Any license provided herein, whether
// implied or otherwise, applies only to this software file.  Patent
// licenses, if any, provided herein do not apply to combinations of
// this program with other software, or any other product whatsoever.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
//
// ====================================================================
//
//  Description:
// ==============
//    Any prototype, definition that does not fall into any category 
// are clustered here 
//
//  o. Analyze_pu_attr: Reveal more __attribute__(()) semantic 
//       This function should be called after :
//         - the CFG is constucted properly all kind of memory 
//           disambiguation is done (so that the analysis can 
//           take advantage of these info)
//       and before:
//         - HSSA construction 
//
// ====================================================================
#ifndef opt_misc_INCLUDED
#define opt_misc_INCLUDED

void Analyze_pu_attr (OPT_STAB* opt_stab, ST* pu_st);

#endif //opt_misc_INCLUDED
