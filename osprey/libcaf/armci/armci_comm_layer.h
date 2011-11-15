/*
 ARMCI Communication Layer for supporting Coarray Fortran

 Copyright (C) 2009-2010 University of Houston.

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

 Contact information:
 http://www.cs.uh.edu/~hpctools
*/


#ifndef ARMCI_COMM_LAYER_H
#define ARMCI_COMM_LAYER_H

/* init */
void comm_init();

/* inline functions */
unsigned long comm_get_proc_id();
unsigned long comm_get_num_procs();

/* coarray read/write */
void comm_read(void *src, void *dest, unsigned long xfer_size, unsigned long proc);
void comm_write(void *dest, void *src, unsigned long xfer_size, unsigned long proc);
void comm_read_src_str(void *src, void *dest, unsigned int ndim, unsigned long *src_strides, unsigned long *src_extents, unsigned long proc);
void comm_write_dest_str(void *dest, void *src, unsigned int ndim, unsigned long *dest_strides, unsigned long *dest_extents, unsigned long proc);
void comm_write_full_str (void * dest, void *src, unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents, 
        unsigned int src_ndim, unsigned long *src_strides,
        unsigned long *src_extents, unsigned long proc);

/* shared memory management */
unsigned long allocate_static_coarrays(); /*TBD */
static void *get_remote_address(void *src, unsigned long img);

/* malloc & free */
void* comm_malloc(size_t size);
void comm_free(void* ptr);

/* barriers */
void comm_barrier_all();
void comm_sync_images(int *image_list, int image_count);

/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize();

#endif
