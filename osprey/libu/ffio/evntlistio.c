/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

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


#pragma ident "@(#) libu/ffio/evntlistio.c	92.2	10/07/99 22:15:19"


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#if	!defined(__mips) && !defined(_LITTLE_ENDIAN)
#include <sys/iosw.h>
#include <sys/listio.h>
#endif
#include <ffio.h>
#include "listio_mips.h"
#include "evntio.h"

/*
 * _evnt_listio
 *
 * Log a listio operation.
 *
 * Input:
 *	cmd    - command  LC_START or LC_WAIT
 * 	list   - pointer to array of structures (listreq) that describes
 *               request
 *	nreq   - number of I/O requests
 *
 * Output:
 *      ret     - return value from flushrtn
 */
int
_evnt_listio(int cmd, struct fflistreq *list, int nreq, struct ffsw *iostat)
{
#if 1
	fprintf(stderr, "*** Notice: the Cray event layer is not currently supported.\n");
	fprintf(stderr, "*** Please contact PathScale if you need this functionality.\n");
	abort();
#else
	struct fdinfo *llfio;
	struct evnt_f *evnt_info;
	int     status;
	struct fdinfo *fio;
	int     ret;
	int     i;
	struct evnt_async_tracker *this_tracker;
	struct rw_stats *rwinfo;
	size_t  nbytes;
	int     save_pos;
	rtc_t   start_rtc, finish_rtc;
	struct ffsw log_stat;
	int     log_ubc = 0;


	/* 
	 * for now assume all requests are for a
	 * single descriptor, the first one
 	 */

#ifdef __mips
	fio = list[0].li_ffioptr;
#else
	fio = (struct fdinfo *) list[0].li_fildes;
#endif
	evnt_info = (struct evnt_f *) (fio->lyr_info);
	llfio = fio->fioptr;

	start_rtc = RTC();
	if ((cmd !=LC_START) && (cmd != LC_WAIT)){
		_SETERROR(iostat, FDC_ERR_REQ, 0);
		return -1;
	}
	if (nreq)
		DO_NEXT_LISTIO(llfio, cmd, list, nreq, ret, iostat)
	finish_rtc = RTC();

	evnt_info->listio_time += (finish_rtc - start_rtc);

	if (evnt_info->fake_sds) {
	    if (list[0].li_opcode == LO_READ)
		evnt_info->listio_read.time += (finish_rtc - start_rtc);
	    else 	  /* implied LO_WRITE */
	        evnt_info->listio_write.time += (finish_rtc - start_rtc);
	}

	/* 
	 * do all the current position stuff 
	 */
	for (i = 0; i < nreq; i++) {	
	    if (list[i].li_flags & LF_LSEEK)
		evnt_info->cur_pos = list[i].li_offset;
	    nbytes = list[i].li_nbyte;

	    if (ILL_FORMED(evnt_info->cur_pos, nbytes, (CPTR2BP(list[i].li_buf)))) {
		if (list[i].li_opcode == LO_READ) {
		    if (cmd == LC_START)
			evnt_info->listio_reada.ill_formed++;
		    else		/* implied LO_WAIT */
			evnt_info->listio_read.ill_formed++;
		} else {      /* implied LO_WRITE */
		    if (cmd == LC_START)
		        evnt_info->listio_writea.ill_formed++;
		    else 	/* implied LC_WAIT */
			evnt_info->listio_write.ill_formed++;
		    }
	    }

	    if (list[i].li_opcode == LO_READ) {
		evnt_info->cur_pos += list[i].li_nbyte;
		if (evnt_info->cur_pos > evnt_info->cur_size)
		    evnt_info->cur_pos = evnt_info->cur_size;
	    } else {		/* implied LO_WRITE */
		evnt_info->cur_pos += list[i].li_nbyte;
		EVNT_CHECK_SIZE;
	    }
	}

#ifndef __mips
	if (evnt_info->optflags.trace) {
	    int     record[3];

	    EVNT_LOCK_ON;
	    record[0] = (_evnt_LISTIO | evnt_info->fd | nreq);
	    record[1] = cmd;
	    status = EVNT_XR_WRITE(record, sizeof(int), 2);
	    status = EVNT_XR_WRITE(list, sizeof(struct fflistreq), nreq);
	    record[0] = start_rtc;
	    record[1] = finish_rtc;
	    record[2] = ret;
	    status = EVNT_XR_WRITE(record, sizeof(int), 3);
	    save_pos = EVNT_XR_TELL();
	    record[0] = record[1] = record[2] = 0;

	    if (cmd == LC_START) {
		for (i = 0; i < nreq; i++) {
		    status = EVNT_XR_WRITE(record, sizeof(int), 3);
		    if (list[i].li_flags & LF_LSEEK)
			evnt_info->open_info.logged.listio_seek++;
			if (list[i].li_opcode == LO_READ)
			    evnt_info->open_info.logged.listio_reada++;
			else if (list[i].li_opcode == LO_WRITE)
			    evnt_info->open_info.logged.listio_writea++;
		}
	    } else if (cmd == LC_WAIT) {
		for (i = 0; i < nreq; i++) {
		    record[2] = list[i].li_status->sw_count;
		    status = EVNT_XR_WRITE(record, sizeof(int), 3);
		    if (list[i].li_flags & LF_LSEEK)
			evnt_info->open_info.logged.listio_seek++;
			if (list[i].li_opcode == LO_READ)
			    evnt_info->open_info.logged.listio_read++;
			else if (list[i].li_opcode == LO_WRITE)
			    evnt_info->open_info.logged.listio_write++;
		}
	    }
	    INC_GLOBAL_LOG_COUNT(listio);
	    EVNT_LOCK_OFF;
	}
#endif
	evnt_info->counts.listio++;
	evnt_info->counts.total++;

	if (cmd == LC_START) {
	    for (i = 0; i < nreq; i++) {
		if ((list[i].li_status->sw_flag != 0) &&
		    (list[i].li_status->sw_stat != 0)) {

		    if (list[i].li_opcode == LO_READ)
			rwinfo = &evnt_info->listio_reada;
		    else
			rwinfo = &evnt_info->listio_writea;

		    rwinfo->delivered += list[i].li_status->sw_count;
		    this_tracker = evnt_info->async_tracker;

		    while (this_tracker) {
			if (this_tracker->mode != TRACKER_FREE &&
			    this_tracker->stat == list[i].li_status) {
			    rwinfo->current--;
			    rwinfo->all_hidden++;
			    this_tracker->mode = TRACKER_FREE;
			    this_tracker->stat = NULL;
			    this_tracker->requested = -1;
			    break;
			}
			this_tracker = this_tracker->next_tracker;
		    }
		}
	    }
	}

	this_tracker = NULL;
	for (i = 0; i < nreq; i++) {
	    if (list[i].li_flags & LF_LSEEK)
		evnt_info->counts.listio_seek++;

	    nbytes = list[i].li_nbyte;
	    if (cmd == LC_START) {
		if (list[i].li_opcode == LO_READ) {
		    evnt_info->counts.listio_reada++;
		    rwinfo = &evnt_info->listio_reada;
		    this_tracker = _evnt_get_tracker(evnt_info,
				  		    list[i].li_status, 
						    TRACKER_LISTIO_READA,
						    nbytes);
		} else {	/* implied LO_WRITE */
		    evnt_info->counts.listio_writea++;
		    rwinfo = &evnt_info->listio_writea;
		    this_tracker = _evnt_get_tracker(evnt_info,
						    list[i].li_status,
						    TRACKER_LISTIO_WRITEA,
						    nbytes);
		}
#if	!defined(__mips) && !defined(_LITTLE_ENDIAN)
		this_tracker->logpos = save_pos + (i) * (sizeof(int)*3);
#endif

	    } else {		/* implied LC_WAIT */
		if (list[i].li_opcode == LO_READ) {
		    evnt_info->counts.listio_read++;
		    rwinfo = &evnt_info->listio_read;
		} else if (list[i].li_opcode == LO_WRITE) {
		    evnt_info->counts.listio_write++;
		    rwinfo = &evnt_info->listio_write;
		}
		rwinfo->delivered += list[i].li_status->sw_count;
	    }

	    rwinfo->requested += nbytes;
	    rwinfo->min = (nbytes < rwinfo->min) ? nbytes : rwinfo->min;
	    rwinfo->max = (nbytes > rwinfo->max) ? nbytes : rwinfo->max;
	}

	return (ret);
#endif
}
