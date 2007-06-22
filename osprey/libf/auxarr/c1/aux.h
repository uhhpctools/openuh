/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License 
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU Lesser General Public 
  License along with this program; if not, write the Free Software 
  Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, 
  USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


/* USMID @(#) libf/auxarr/c1/aux.h	92.0	10/08/98 14:30:10 */
  

	/* This structure describes the _infoblk created by segldr */
	typedef struct {
		unsigned	vers:7,
				unused:25,
				len:32;
		unsigned	infoblk;
		unsigned	checksum;
		unsigned	loadmdy;
		unsigned	loadhms;
		unsigned	loadpid;
		unsigned	loadpvr;
		unsigned	loadosvr;
		unsigned	loadudt;
		unsigned	preset;
		unsigned	basetext:32,
				basedata:32;
		unsigned	textlen:32,
				datalen:32;
		unsigned	bsslen:32,
				zerolen:32;
		unsigned	cdatalen:32,
				lmlen:32;
		unsigned	vmlen:32,
				mmbase:32;
		unsigned	heapinit:32,
				heapinc:32;
		unsigned	stakinit:32,
				stakinc:32;
		unsigned	usxfirst:32,
				usxlast:32;
		unsigned 	mtptr:32,
				compresptr:32;
		unsigned	clockspeed:32,
				environptr:32;
		unsigned	segptr:32,
				unused3:32;
}info;


typedef struct {
	int begcompadr;		/* First compiler address */
	int endcompadr;		/* Last compiler address */
	int ssdadr;		/* Corresponding ssd address */
}trans;

#define TRUE 1
#define FALSE 0

#ifndef NULL
#define NULL 0
#endif

#define MSG1 "Could not allocate memory in $auxinit "
#define MSG2 "Failed to allocate space on SDS: "
#define MSG3 "Internal library error in $auxread "
#define MSG4 "sswrite failed: "
#define MSG5 "ssread failed: "
#define MSG6 "Could not allocate memory "
#define MSG7 "Sdsfree() failed: "
#define MSG8 "Bad address given to $sdsfree "
#define MSG9 "Bad address given to $auxread "
