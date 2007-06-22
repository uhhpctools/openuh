/*
 *  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
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



/* USMID:  "\n@(#)5.0_pl/headers/p_dcls.h	5.1	04/29/99 21:22:31\n" */

enum		attr_values	{Allocatable_Attr,
				 Automatic_Attr,
				 Co_Array_Attr,
				 Dimension_Attr,
				 External_Attr,
				 Intent_Attr,
				 Intrinsic_Attr,
				 Optional_Attr,
				 Parameter_Attr,
				 Pointer_Attr,
				 Private_Attr,
				 Public_Attr,
				 Save_Attr,
				 Target_Attr,
				 Volatile_Attr,
				 End_Attr = Volatile_Attr };

typedef	enum	 attr_values	attr_type;

static	char	*attr_str[Volatile_Attr+1] =	{"ALLOCATABLE",
						 "AUTOMATIC",
						 "CO-ARRAY",
						 "DIMENSION",
						 "EXTERNAL",
						 "INTENT",
						 "INTRINSIC",
						 "OPTIONAL",
						 "PARAMETER",
						 "POINTER",
						 "PRIVATE",
						 "PUBLIC",
						 "SAVE",
						 "TARGET",
						 "VOLATILE" };

static	long	 err_attrs[Volatile_Attr+1] =       {

		 /* Allocatable_Attr */		((1 << Allocatable_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << External_Attr) |
#ifndef KEY /* Bug 6845 */
		 /* Allocatable now allowed on dummy args */
						 (1 << Intent_Attr) |
#endif /* KEY Bug 6845 */
						 (1 << Intrinsic_Attr) |
#ifndef KEY /* Bug 6845 */
		 /* Allocatable now allowed on dummy args */
						 (1 << Optional_Attr) |
#endif /* KEY Bug 6845 */
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr)),

		 /* Automatic_Attr */		((1 << Allocatable_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Co_Array_Attr) |
						 (1 << External_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Optional_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Save_Attr)),

		 /* Co_Array_Attr */		((1 << Automatic_Attr) |
						 (1 << Co_Array_Attr) |
						 (1 << External_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Optional_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr)),

		 /* Dimension_Attr */		 (1 << Dimension_Attr),

		 /* External_Attr */		((1 << Allocatable_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Co_Array_Attr) |
						 (1 << External_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr) |
						 (1 << Save_Attr) |
						 (1 << Target_Attr) |
						 (1 << Volatile_Attr)),

		 /* Intent_Attr */		(
#ifndef KEY /* Bug 6845 */
		 /* Allocatable now allowed on dummy args */
		                                 (1 << Allocatable_Attr) |
#endif /* KEY Bug 6845 */
						 (1 << Automatic_Attr) |
						 (1 << External_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr) |
						 (1 << Save_Attr)),

		 /* Intrinsic_Attr */		((1 << Allocatable_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Co_Array_Attr) |
						 (1 << External_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Optional_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr) |
						 (1 << Save_Attr) |
						 (1 << Target_Attr) |
						 (1 << Volatile_Attr)),

		 /* Optional_Attr */		(
#ifndef KEY /* Bug 6845 */
		 /* Allocatable now allowed on dummy args */
		                                 (1 << Allocatable_Attr) |
#endif /* KEY Bug 6845 */
						 (1 << Automatic_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Optional_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Save_Attr)),

		 /* Parameter_Attr */		((1 << Allocatable_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Co_Array_Attr) |
						 (1 << External_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Optional_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr) |
						 (1 << Save_Attr) |
						 (1 << Target_Attr) |
						 (1 << Volatile_Attr)),

		 /* Pointer_Attr */		((1 << Allocatable_Attr) |
						 (1 << Co_Array_Attr) |
						 (1 << External_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr) |
						 (1 << Target_Attr)),

		 /* Private_Attr */		((1 << Private_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Public_Attr)),

		 /* Public_Attr */		((1 << Private_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Public_Attr)),

		 /* Save_Attr */		((1 << External_Attr) |
						 (1 << Automatic_Attr) |
						 (1 << Intent_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Optional_Attr) |
						 (1 << Save_Attr)),

		 /* Target_Attr */		((1 << External_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Parameter_Attr) |
						 (1 << Pointer_Attr) |
						 (1 << Target_Attr)),

		 /* Volatile_Attr */		((1 << External_Attr) |
						 (1 << Intrinsic_Attr) |
						 (1 << Parameter_Attr))
					       };
