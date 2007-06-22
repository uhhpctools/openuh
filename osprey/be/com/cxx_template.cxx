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


//-*-c++-*-
/* ====================================================================
 * ====================================================================
 *
 * Module: cxx_template.cxx
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/com/cxx_template.cxx,v $
 *
 * Revision history:
 *  8-SEP-94 shin - Original Version
 *
 * Description:
 *
 * ====================================================================
 * ====================================================================
 */
#define cxx_template_CXX	"cxx_template.cxx"
//	rcs_id NOT defined here so that it won't get duplicated 
//	because this is template, hence gets included implicitly

#include "defs.h"
#include "errors.h"
#include "erglob.h"
#include "cxx_memory.h"
#include "cxx_template.h"

#define MIN_ARRAY_SIZE 16

#if 0
template <class T >
DYN_ARRAY<T>::DYN_ARRAY(void)
{
  _lastidx = -1;
  _size = 0;
  _array = NULL;
  _mpool = NULL;
}

template <class T >
DYN_ARRAY<T>::DYN_ARRAY(MEM_POOL *pool)
{
  _lastidx = -1;
  _size = 0;
  _array = NULL;
  _mpool = pool;
}

template <class T >
DYN_ARRAY<T>::~DYN_ARRAY()
{
  Free_array();
}

/* must guarantee a min. non-zero size */
template <class T>
void
DYN_ARRAY<T>::Alloc_array(mUINT32 arr_size)
{
   _size = arr_size > MIN_ARRAY_SIZE ? arr_size : MIN_ARRAY_SIZE;
  _array = (T*)MEM_POOL_Alloc(_mpool, _size * sizeof(T));
  if ( _array == NULL ) ErrMsg ( EC_No_Mem, "DYN_ARRAY::Alloc_array" );
}

/* min. size is 1, instead of MIN_ARRAY_SIZE */
template <class T>
void
DYN_ARRAY<T>::Force_Alloc_array (mUINT32 arr_size)
{
    _size = arr_size > 1 ? arr_size : 1;
    _array = (T*)MEM_POOL_Alloc(_mpool, _size * sizeof(T));
    if ( _array == NULL ) ErrMsg ( EC_No_Mem, "DYN_ARRAY::Alloc_array" );
} 

template <class T>
void
DYN_ARRAY<T>::Realloc_array(mUINT32 new_size)
{
  _array = (T*)MEM_POOL_Realloc(_mpool,
			       _array,
			       sizeof(T) * _size,
			       sizeof(T) * new_size);
  if ( _array == NULL ) ErrMsg ( EC_No_Mem, "DYN_ARRAY::Realloc_array" );
  _size = new_size;
}

template <class T>
void
DYN_ARRAY<T>::Free_array()
{
  if (_array != NULL) {
      MEM_POOL_FREE(_mpool,_array);
      _array = NULL;
      _size = 0;
  }
}

template <class T>
void
DYN_ARRAY<T>::Bzero_array()
{
  if (_array != NULL) bzero(_array,sizeof(T) * _size);
}

template <class T>
DYN_ARRAY<T>&
DYN_ARRAY<T>::operator = (const DYN_ARRAY<T>& a)
{
  if (_size != a._size) Realloc_array(a._size);
  _lastidx = a._lastidx;
  memcpy (_array, a._array, a._size * sizeof(T));
  return *this;
}

template <class T >
mUINT32
DYN_ARRAY<T>::Newidx()
{
  _lastidx++;
  if (_lastidx >= _size) {
    // overflow the allocated array, resize the array
    if (_array == NULL) {
	Alloc_array (MIN_ARRAY_SIZE); // Alloc_array guarantees non-zero size
    } else {
	Realloc_array (_size * 2);
    }
  }
  return _lastidx;
}

template <class T >
void
DYN_ARRAY<T>::Initidx(UINT32 idx)
{
  _lastidx=idx;
  if (_lastidx >= _size) {
    // overflow the allocated array, resize the array
    if (_array != NULL) {
      Free_array();
    }
    Alloc_array(_lastidx + 1);
  }
}

template <class T >
void
DYN_ARRAY<T>::Setidx(UINT32 idx)
{
  _lastidx=idx;
  if (_lastidx >= _size) {
    // overflow the allocated array, resize the array
    if (_array == 0)
      Alloc_array(_lastidx + 1);
    else {
      INT32 new_size = _size * 2;
      while (new_size < _lastidx + 1) new_size *= 2;
      Realloc_array(new_size);
    }
  }
}

template <class T>
void STACK<T>::Settop(const T& val)
{
  INT32 idx = _stack.Lastidx();
  
  Is_True(idx >= 0, ("STACK::Settop(): Stack Empty"));
  _stack[idx] = val;
}


template <class T>
T& STACK<T>::Top_nth(const INT32 n) const
{
  INT32 idx = _stack.Lastidx();
  
  Is_True(idx >= n, ("STACK::Top_nth(): Access beyond stack bottom"));
  return _stack[idx - n];
}


template <class T>
T& STACK<T>::Bottom_nth(const INT32 n) const
{
  INT32 idx = _stack.Lastidx();
  
  Is_True(n <= idx, ("STACK::Bottom_nth(): Access beyond stack top"));
  return _stack[n];
}

    
template <class T>
T& STACK<T>::Top(void) const
{
  INT32 idx = _stack.Lastidx();
  
  Is_True(idx >= 0, ("STACK::Top(): Stack Empty"));
  return _stack[idx];
}

template <class T>
BOOL STACK<T>::Is_Empty(void) const
{
  return _stack.Lastidx() < 0;
}


template <class CONTAINER, class PREDICATE>
void Remove_if(CONTAINER& container, PREDICATE predicate)
{
  CONTAINER::CONTAINER_NODE *prev = NULL, *curr, *next;
  for (curr = container.Head();  curr != NULL;  curr = next) {
    next = curr->Next();
    if (predicate(curr)) {
      if (prev == NULL)
	container.Set_Head(next);
      else
	prev->Set_Next(next);
    } else {
      prev = curr;
    }
  }
  if (prev == NULL)
    container.Set_Tail(container.Head());
  else
    container.Set_Tail(prev);
}
#endif
