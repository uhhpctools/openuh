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
#ifndef anl_srcpos_INCLUDED
#define anl_srcpos_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_srcpos.h
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/prompf_anl/anl_srcpos.h,v $
//
// Description:
//
//    An object oriented interface to srcpos utilities.  Note that
//    all relational comparison operators return FALSE for different
//    Filenum(), since we assume no order between files.
//
// ==============================================================
// ==============================================================

#include "srcpos.h"

class ANL_SRCPOS
{
private:

   USRCPOS _usrcpos;

public:

   // ============== Constructors & Destructors ==============

   ANL_SRCPOS()              {USRCPOS_srcpos(_usrcpos) = 0;}
   ANL_SRCPOS(SRCPOS srcpos) {USRCPOS_srcpos(_usrcpos) = srcpos;}
   ANL_SRCPOS(WN *stmt)      
   {
      if (stmt == NULL)
	 USRCPOS_srcpos(_usrcpos) = 0;
      else
	 USRCPOS_srcpos(_usrcpos) = WN_linenum(stmt);
   }


   // =============== Accessors ===============

   INT32 Filenum() const {return USRCPOS_filenum(_usrcpos);}
   INT32 Linenum() const {return USRCPOS_linenum(_usrcpos);}
   INT32 Column()  const {return USRCPOS_column(_usrcpos);}


   // =============== Writing to string buffer ===============

   void Write(ANL_CBUF *cbuf);
   void Write_Filenum(ANL_CBUF *cbuf) const {cbuf->Write_Int(Filenum());}
   void Write_Linenum(ANL_CBUF *cbuf) const {cbuf->Write_Int(Linenum());}
   void Write_Column(ANL_CBUF *cbuf)  const {cbuf->Write_Int(Column());}


   // =============== Assignment Operators ===============

   ANL_SRCPOS &operator=(const ANL_SRCPOS &s)
   {
      _usrcpos.srcpos = s._usrcpos.srcpos;
      return *this;
   }
   ANL_SRCPOS &operator+=(INT32 lineincr) 
   {
      USRCPOS_linenum(_usrcpos) += lineincr;
      return *this;
   }
   ANL_SRCPOS &operator-=(INT32 linedecr) 
   {
      USRCPOS_linenum(_usrcpos) -= linedecr;
      return *this;
   }

   // =============== Comparison Operators ===============

   
   BOOL operator==(const ANL_SRCPOS &s)
   {
      return s._usrcpos.srcpos == _usrcpos.srcpos;
   }
   BOOL operator!=(const ANL_SRCPOS &s)
   {
      return s._usrcpos.srcpos == _usrcpos.srcpos;
   }
   BOOL operator<(const ANL_SRCPOS &s);
   BOOL operator<=(const ANL_SRCPOS &s);
   BOOL operator>(const ANL_SRCPOS &s);
   BOOL operator>=(const ANL_SRCPOS &s);

}; // class ANL_SRCPOS

#endif // anl_srcpos_INCLUDED
