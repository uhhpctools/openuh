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


/* -*-Mode: c++;-*- (Tell emacs to use c++ mode) */

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <errno.h>    // for sys_errlist
#include <stdio.h>    // for put/get and print/read
#include <libgen.h>   // for basename()
#include <sys/stat.h>
#include "anl_common.h"    // For ANL_DIAGNOSTICS, etc.
#include "anl_file_mngr.h"


// Avoid errors due to uses of "int" in stdio.h macros.
//
#undef int


// =================== Static Utilities ===================
// ========================================================

const INT ANL_FILE_MNGR::_obuf_size;


// =============== Hidden Member Functions ================
// ========================================================


void 
ANL_FILE_MNGR::_Concat(char       *buf,
		       INT         max_chars,
		       const char *string[],
		       INT         num_strings)
{
   INT char_cntr = 0;

   for (INT i=0; i < num_strings; i++)
   {
      const char *s = string[i];

      for (INT j=0; char_cntr < max_chars && s[j] != '\0'; j++)
	 buf[char_cntr++] = s[j];
   }
   if (char_cntr < max_chars)
      buf[char_cntr] = '\0';
   else
      buf[max_chars-1] = '\0';
} // ANL_FILE_MNGR::_Concat


UINT64
ANL_FILE_MNGR::_Get_Decimal_Number(INT ch)
{
   // Returns UINT64_MAX if illegal character
   //
   switch (ch)
   {
   case '0':
      return 0;
   case '1':
      return 1;
   case '2':
      return 2;
   case '3':
      return 3;
   case '4':
      return 4;
   case '5':
      return 5;
   case '6':
      return 6;
   case '7':
      return 7;
   case '8':
      return 8;
   case '9':
      return 9;
   default:
      return UINT64_MAX; // illegal character
   }
} // ANL_FILE_MNGR::_Get_Decimal_Number


UINT64
ANL_FILE_MNGR::_Get_Hex_Number(INT ch)
{
   // Returns UINT64_MAX if illegal character
   //
   switch (ch)
   {
   case 'a':
      return 10;
   case 'b':
      return 11;
   case 'c':
      return 12;
   case 'd':
      return 13;
   case 'e':
      return 14;
   case 'f':
      return 15;
   case 'A':
      return 10;
   case 'B':
      return 11;
   case 'C':
      return 12;
   case 'D':
      return 13;
   case 'E':
      return 14;
   case 'F':
      return 15;
   case '0':
      return 0;
   case '1':
      return 1;
   case '2':
      return 2;
   case '3':
      return 3;
   case '4':
      return 4;
   case '5':
      return 5;
   case '6':
      return 6;
   case '7':
      return 7;
   case '8':
      return 8;
   case '9':
      return 9;
   default:
      return UINT64_MAX; // illegal character
   }
} // ANL_FILE_MNGR::_Get_Hex_Number


BOOL 
ANL_FILE_MNGR::_Exists(const char *name)
{
   INT         st;
   struct stat sbuf;
   st = stat(name, &sbuf);
   if (st == -1 && (errno == ENOENT || errno == ENOTDIR))
      return FALSE;
   else
      return TRUE;
} // ANL_FILE_MNGR::_Exists


void 
ANL_FILE_MNGR::_General_Check(BOOL c, const char *proc_name, const char *msg)
{
   if (!c)
   {
      char        strbuf[500];
      const char *strlist[5] = {proc_name, msg, " (", _name, ")"};
      _Concat(strbuf, 500, strlist, 5);
      _diag->Warning(strbuf);
   }
} // ANL_FILE_MNGR::_General_Check


void
ANL_FILE_MNGR::_Not_Open_Check(const char *proc_name, const char *to_be_opened)
{
   if (File_Is_Open())
   {
      char        strbuf[500];
      const char *strlist[5] = {proc_name,
				" will close unexpected open file ",
				_name,
				" and then open.  ",
				to_be_opened};
      _Concat(strbuf, 500, strlist, 5);
      _diag->Warning(strbuf);
      Close_File();
   }
} // ANL_FILE_MNGR::_Not_Open_Check


void
ANL_FILE_MNGR::_Is_Open_Check(const char *proc_name)
{
   if (!File_Is_Open())
   {
      char        strbuf[500];
      const char *strlist[2] = {proc_name,
				" expected a file to have been opened"};
      _Concat(strbuf, 500, strlist, 2);
      _diag->Error(strbuf);
   }
} // ANL_FILE_MNGR::_Is_Open_Check


void 
ANL_FILE_MNGR::_Overwrite_Warning(const char *proc_name, const char *filename)
{
   if (_Exists(filename))
   {
      char        strbuf[500];
      const char *strlist[4] = {filename,
				" will be overwritten (by ",
			        proc_name,
                                ")"};
      _Concat(strbuf, 500, strlist, 4);
      _diag->Warning(strbuf);
   }
} // ANL_FILE_MNGR::_Overwrite_Warning


void ANL_FILE_MNGR::_Write_Obuf()
{
   INT status;

   _obuf[_next_obuf] = '\0';
   status = fputs(_obuf, _file);
   _next_obuf = 0;
   _General_Check(status != EOF,
		  "ANL_FILE_MNGR::_Write_Obuf", 
		  "cannot write to file");
} // ANL_FILE_MNGR::_Write_Obuf


// =============== Public Member Functions ================
// ========================================================

void 
ANL_FILE_MNGR::Open_Read(const char *name)
{
   _Not_Open_Check("ANL_FILE_MNGR::Open_Read", name);
   _file = fopen(name, "r");
   _Is_Open_Check("ANL_FILE_MNGR::Open_Read");
   _name = name;
   _next_ch = getc(_file);
   _General_Check(!ferror(_file),
		  "ANL_FILE_MNGR::Open_Read", 
		  "cannot read first character in file");
} // ANL_FILE_MNGR::Open_Read


void 
ANL_FILE_MNGR::Open_Create(const char *name)
{
   _Not_Open_Check("ANL_FILE_MNGR::Open_Create", name);
   _Overwrite_Warning("ANL_FILE_MNGR::Open_Create", name);
   _file = fopen(name, "w");
   _Is_Open_Check("ANL_FILE_MNGR::Open_Create");
   _name = name;
} // ANL_FILE_MNGR::Open_Create


void 
ANL_FILE_MNGR::Open_Append(const char *name)
{
   _Not_Open_Check("ANL_FILE_MNGR::Open_Append", name);
   _file = fopen(name, "a");
   _Is_Open_Check("ANL_FILE_MNGR::Open_Append");
   _name = name;
} // ANL_FILE_MNGR::Open_Append


void 
ANL_FILE_MNGR::Close_File()
{
   INT32 status;

   if (_next_obuf > 0)
      _Write_Obuf();
   status = fclose(_file);
   _General_Check(status == 0,
		  "ANL_FILE_MNGR::Close_File", 
		  "cannot close file");
   _file = NULL;
   _name = NULL;
} // ANL_FILE_MNGR::Close_File


void 
ANL_FILE_MNGR::Close_And_Remove_File()
{
   INT32 status;

   if (_next_obuf > 0)
      _Write_Obuf();
   status = fclose(_file);
   _General_Check(status == 0,
		  "ANL_FILE_MNGR::Close_File", 
		  "cannot close file");
   remove(_name);
   _file = NULL;
   _name = NULL;
} // ANL_FILE_MNGR::Close_And_Remove_File


char 
ANL_FILE_MNGR::Read_Char()
{
   INT32 ch;

   _General_Check(!End_Of_File(),
		  "ANL_FILE_MNGR::Read_Char", 
		  "attempt to read beyond the end of the file");
   ch = _next_ch;
   _next_ch = getc(_file);
   return ch;
} // ANL_FILE_MNGR::Read_Char


UINT64 
ANL_FILE_MNGR::Read_Uint64(BOOL as_hex)
{
   INT32  num_digits = 0;
   UINT64 result = 0;
   UINT64 digit;

   _General_Check(!End_Of_File(),
		  "ANL_FILE_MNGR::Read_Uint64", 
		  "attempt to read beyond the end of the file");
   if (as_hex)
   {
      if (_next_ch == '0')
      {
	 _next_ch = getc(_file);
	 if (_next_ch == 'x' || _next_ch == 'X')
	    _next_ch = getc(_file);
	 else
	    num_digits++;
      }

      for (digit = _Get_Hex_Number(_next_ch);
	   digit != UINT64_MAX;
	   digit = _Get_Hex_Number(_next_ch))
      {
	 _next_ch = getc(_file);
	 result = result*16 + digit;
	 num_digits++;
      }
      _General_Check(num_digits > 0 && num_digits <= 17,
		     "ANL_FILE_MNGR::Read_Uint64", 
		     "unexpected syntax");
   }
   else
   {
      for (digit = _Get_Decimal_Number(_next_ch);
	   digit != UINT64_MAX;
	   digit = _Get_Decimal_Number(_next_ch))
      {
	 _next_ch = getc(_file);
	 result = result*10 + digit;
	 num_digits++;
      }
      _General_Check(num_digits > 0 && num_digits <= 17,
		     "ANL_FILE_MNGR::Read_Uint64", 
		     "unexpected syntax");
   }

   return result;
} // ANL_FILE_MNGR::Read_Uint64


void 
ANL_FILE_MNGR::Write_Char(char c)
{
   if (_next_obuf >= _obuf_size)
      _Write_Obuf();
      
   _obuf[_next_obuf++] = c;
} // ANL_FILE_MNGR::Write_Char


void 
ANL_FILE_MNGR::Write_String(const char *s)
{
   if (s != NULL)
      for (const char *p = s; *p != '\0'; p++)
      {
	 if (_next_obuf >= _obuf_size)
	    _Write_Obuf();
	 _obuf[_next_obuf++] = *p;
      }
} // ANL_FILE_MNGR::Write_String
