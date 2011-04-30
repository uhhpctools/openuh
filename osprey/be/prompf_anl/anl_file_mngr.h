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
#ifndef anl_file_mngr_INCLUDED
#define anl_file_mngr_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_file_mngr.h
// $Revision: 1.1 $
// $Date: 2005/07/27 02:13:44 $
// $Author: kevinlo $
// $Source: /depot/CVSROOT/javi/src/sw/cmplr/be/prompf_anl/anl_file_mngr.h,v $
//
// Description:
//
//    A file handler, which reports error messages using an abstraction
//    named DIAGNOSTICS.  The interface of DIAGNOSTICS is expected to
//    include the following:
//
//         Warning: (char *) -> void
//         Error:   (char *) -> void
//         Fatal:   (char *) -> void
//
//    where a Fatal call is not expected to return, but is expected to
//    terminate execution.  An Error or a Warning will return.  The state
//    after an error may not be as expected.
//
//    This implementation is based on the <stdio.h> facilities and 
//    exports only what we currently need.  The interface can be 
//    extended and generalized to fit any future needs.  Intended 
//    use for reading from a file (similar for writing to a file):
//
//       ANL_FILE_MNGR file_mngr(diag);
//       file_mngr.Open_Read(filename);
//       while (!file_mngr.Error_Status() && !file_mngr.End_Of_File())
//       {
//          char c = file_mngr.Read_Char();
//          .... c ...
//       }
//       file_mngr.Close_File();
//
//    This abstraction does not buffer output beyond what is provided
//    by default through the <stdio.h> utilities, so any such buffering
//    must occur outside of this abstraction.  Such buffering may be
//    desirable, since this abstraction will perform fairly extensive
//    tests on all IO operations.
//
// ==============================================================
// ==============================================================


class ANL_DIAGNOSTICS;

class ANL_FILE_MNGR
{
private:

   ANL_DIAGNOSTICS *_diag;
   const char      *_name;
   FILE            *_file;
   INT32            _next_ch;

   static const INT _obuf_size = 513;    // Size of output buffer
   INT              _next_obuf;          // Next available char in _obuf
   char             _obuf[_obuf_size+1]; // Output buffer

   static void _Concat(char       *buf,
		       INT         max_chars,
		       const char *string[],
		       INT         num_strings);

   static UINT64 _Get_Decimal_Number(INT ch);
   static UINT64 _Get_Hex_Number(INT ch);
   static BOOL   _Exists(const char *name);

   void _General_Check(BOOL c, const char *proc_name, const char *msg);
   void _Not_Open_Check(const char *proc_name, const char *to_be_opened);
   void _Is_Open_Check(const char *proc_name);
   void _Overwrite_Warning(const char *proc_name, const char *filename);
   void _Write_Obuf();

public:

   ANL_FILE_MNGR(ANL_DIAGNOSTICS *diag):
     _diag(diag),
     _name(NULL),
     _file(NULL),
     _next_ch(EOF),
     _next_obuf(0)
   {}

   ~ANL_FILE_MNGR() { if (_file != NULL) Close_File(); }

   void Open_Read(const char *name);
   void Open_Create(const char *name);
   void Open_Append(const char *name);
   void Close_File();
   void Close_And_Remove_File();

   BOOL   File_Is_Open() {return (_file != NULL);}
   BOOL   End_Of_File() {return (_next_ch == EOF);}
   char   Peek_Char() {return _next_ch;}   // Does not alter stream ptr
   char   Read_Char();   // Returns current char and advances stream ptr
   UINT64 Read_Uint64(BOOL as_hex = FALSE);
   UINT64 Read_Ptr() {return Read_Uint64(TRUE);}
   void   Write_Char(char c);
   void   Write_String(const char *s);
   void   Flush_Write_Buffer() {if (_next_obuf > 0) _Write_Obuf();}

   const char *Name() const {return _name;}
   FILE       *File() {return _file;}
   
}; // ANL_FILE_MNGR


#endif /* anl_file_mngr_INCLUDED */
