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


/* -*-Mode: c++;-*- (Tell emacs to use c++ mode) */
#ifndef anl_diagnostics_INCLUDED
#define anl_diagnostics_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_diagnostics.h
// $Revision: 1.1 $
// $Date: 2005/07/27 02:13:44 $
// $Author: kevinlo $
// $Source: /depot/CVSROOT/javi/src/sw/cmplr/be/prompf_anl/anl_diagnostics.h,v $
//
// Description:
//
//    A class for diagnostics handling in prompf_anl.so.  Note
//    that the "diag_file" must be opened externally to this
//    class, and will not be explicitly closed before program
//    termination upon a fatal error.
//
// ==============================================================
// ==============================================================


class ANL_DIAGNOSTICS
{
private:

   BOOL  _error_condition;
   FILE *_diag_file;

public:

   ANL_DIAGNOSTICS(FILE *diag_file):
   _error_condition(FALSE),
   _diag_file(diag_file)
   {}

   void Warning(const char *msg);
   void Error(const char *msg);
   void Fatal(const char *msg);

   BOOL Error_Was_Reported() const {return _error_condition;}
   void Reset_Error_Condition() {_error_condition = FALSE;}

}; // ANL_DIAGNOSTICS


#endif /* anl_diagnostics_INCLUDED */
