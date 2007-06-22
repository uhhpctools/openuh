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


// -*-C++-*-
// ====================================================================
// ====================================================================
//
// Module: cxx_hash.cxx
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/com/cxx_hash.cxx,v $
//
// Revision history:
//  07-Dec-95 - Merged user-hash version from IPA
//
// Description:
//
// Template member function bodies for template hash map
// implementations.
//
// ====================================================================
// ====================================================================

#ifdef _KEEP_RCS_ID
#define cxx_hash_CXX      "cxx_hash.cxx"
static char *rcs_id = cxx_hash_CXX" $Revision: 1.1.1.1 $";
#endif /* _KEEP_RCS_ID */

#include "erglob.h"
#include "cxx_hash.h"

// ====================================================================
// ====================================================================
//
// HASH_TABLE
//
// This is a simple hash map, with the following attributes:
//
//  1)	The size of the hash table is determined at constructor time.
//
//  2)	The hash table elements are lists of objects.
//
//  3)	The objects in the table's lists are pairs consisting of a
//	signature (key) and a data element to which the key is mapped.
//
//  4)	The hash function is built in as the signature modulo the table
//	size.  Therefore, for example, it will be the pointer value for
//	a string key, and will produce different entries for distinct
//	pointers to the same character string.
//
// ====================================================================
// ====================================================================

#if 0

template <class SIG_TYPE, class DATA_TYPE>
HASH_TABLE<SIG_TYPE,DATA_TYPE> :: HASH_TABLE (
  UINT num_elements,
  MEM_POOL *pool )
{
  typedef HASH_ELEMENT<SIG_TYPE,DATA_TYPE> *HASH_ELEMENTP;
  _pool = pool;
  _num_elements = num_elements;
  _num_entries = 0;
  _data = CXX_NEW_ARRAY(HASH_ELEMENTP ,num_elements,pool);
  for (INT i=0; i<num_elements; i++) {
    _data[i] = (HASH_ELEMENTP) 0;
  }
}

template <class SIG_TYPE, class DATA_TYPE>
void
HASH_TABLE<SIG_TYPE,DATA_TYPE> :: Enter_If_Unique(SIG_TYPE signature, 
						  DATA_TYPE data)
{ 
  typedef HASH_ELEMENT<SIG_TYPE,DATA_TYPE> THIS_HASH_ELEMENT;
  typedef HASH_ELEMENT<SIG_TYPE,DATA_TYPE> *HASH_ELEMENTP;
  HASH_ELEMENTP element = 
    CXX_NEW(THIS_HASH_ELEMENT(signature,data),_pool);
  UINT location = abs((INT)signature) % _num_elements;

  if (_data[location]) { // something is there
    HASH_ELEMENTP iter = _data[location];
    for (; iter != NULL ; iter = iter->_next) {
      if (iter->_signature == signature) {
         return; // not unique
      }
    }
    _data[location]->Add_To_List(element);
  } else {
    _data[location] = element;
  }
  _num_entries++;
}

template <class SIG_TYPE, class DATA_TYPE>
DATA_TYPE
HASH_TABLE<SIG_TYPE,DATA_TYPE> :: Find (
  SIG_TYPE signature ) const
{
  typedef HASH_ELEMENT<SIG_TYPE,DATA_TYPE> *HASH_ELEMENTP;
  HASH_ELEMENTP hash_element = _data[abs((INT)signature) % _num_elements];

  for (; hash_element != NULL; hash_element = hash_element->_next) {
    if (hash_element->_signature == signature) {
      return(hash_element->_data);
    }
  }
  return((DATA_TYPE)NULL);
}

template <class SIG_TYPE, class DATA_TYPE>
void HASH_TABLE<SIG_TYPE,DATA_TYPE> :: Remove (
  SIG_TYPE signature ) 
{
  typedef HASH_ELEMENT<SIG_TYPE,DATA_TYPE> *HASH_ELEMENTP;
  HASH_ELEMENTP hash_element = _data[abs((INT)signature) % _num_elements];

  if (hash_element->_signature == signature) {
    _data[abs((INT)signature) % _num_elements] = hash_element->_next;
    CXX_DELETE(hash_element,_pool);
    _num_entries--;
    return;
  }

  HASH_ELEMENTP prev = hash_element;
  for (hash_element = hash_element->_next; hash_element; 
				hash_element = hash_element->_next) {
    if (hash_element->_signature == signature) {
      prev->_next = hash_element->_next;
      CXX_DELETE(hash_element,_pool);
      _num_entries--;
      return;
    }
    prev = hash_element;
  }
}

template <class SIG_TYPE, class DATA_TYPE>
BOOL
HASH_TABLE_ITER<SIG_TYPE,DATA_TYPE> :: Step (
  SIG_TYPE* sig,
  DATA_TYPE* data )
{
  if (_he && _he->_next) {
    _he = _he->_next;
    *sig = _he->_signature;
    *data = _he->_data;
    return TRUE;
  }
    
  for (_loc++; _loc < _hash->Num_Elements(); _loc++) {
    if (_hash->Data(_loc)) {
      _he = _hash->Data(_loc);
      *sig = _he->_signature;
      *data = _he->_data;
      return TRUE;
    }
  }

  return FALSE;
}

// ====================================================================
// ====================================================================
//
// USER_HASH_TABLE
//
// This is a hash map, very similar to HASH_TABLE, with the following
// attributes (only #4 differs from HASH_TABLE):
//
//  1)	The size of the hash table is determined at constructor time.
//
//  2)	The hash table elements are lists of objects.
//
//  3)	The objects in the table's lists are pairs consisting of a
//	signature (key) and a data element to which the key is mapped.
//
//  4)	The hash function is provided by the user as a function object
//	HASH_FUNC on the KEY_TYPE, and equivalence between keys is
//	also provided by a user function object KEY_EQ.
//
// ====================================================================
// ====================================================================

TEMPLATE_USER_HASH_TABLE
CLASS_USER_HASH_TABLE :: USER_HASH_TABLE
  ( UINT32 num_elements, MEM_POOL *pool )
{
  typedef HASH_ELEMENT < KEY_TYPE, DATA_TYPE > *pHASH_ELEMENT;

  _pool = pool;
  _num_elements = num_elements;
  _num_entries = 0;
  _data = CXX_NEW_ARRAY ( pHASH_ELEMENT, num_elements, pool );
  if ( _data == NULL ) {
    ErrMsg ( EC_No_Mem, "USER_HASH_TABLE::USER_HASH_TABLE" );
  }
  for ( INT i=0; i<num_elements; i++ ) {
    _data[i] = (pHASH_ELEMENT) NULL;
  }
}

TEMPLATE_USER_HASH_TABLE
void
CLASS_USER_HASH_TABLE :: Print ( FILE *f )
{
  HASH_ELEMENT < KEY_TYPE, DATA_TYPE > *elt;

  for ( INT32 i = 0; i < _num_elements; i++ ) {
    if ( _data[i] != NULL ) {
      fprintf ( f, "%2d:", i );
      elt = _data[i];
      while  ( elt != NULL ) {
	fprintf ( f, " k%2d:%s:d%d:n0x%06lx",
		  _hash(elt->_signature) % _num_elements,
		  elt->_signature, elt->_data, elt->_next );
	elt = elt->_next;
      }
      fprintf ( f, "\n" );
    }
  }
}

TEMPLATE_USER_HASH_TABLE
void
CLASS_USER_HASH_TABLE :: Enter_If_Unique(KEY_TYPE key, DATA_TYPE data)
{ 
  typedef HASH_ELEMENT<KEY_TYPE,DATA_TYPE> THIS_HASH_ELEMENT;
  typedef HASH_ELEMENT<KEY_TYPE,DATA_TYPE> *pHASH_ELEMENT;
  pHASH_ELEMENT element = 
	CXX_NEW ( THIS_HASH_ELEMENT(key,data), _pool );
  UINT32 location = _hash ( key ) % _num_elements;

  if ( element == NULL ) {
    ErrMsg ( EC_No_Mem, "USER_HASH_TABLE::Enter_If_Unique" );
  }

  if ( _data[location] ) { // something is there
    pHASH_ELEMENT iter = _data[location];

    for ( ; iter != NULL ; iter = iter->_next ) {
      if ( _equal ( iter->_signature, key ) ) {
        return; // not unique
      }
    }
    element->_next = _data[location];
  }
  _data[location] = element;
  _num_entries++;
}

TEMPLATE_USER_HASH_TABLE
DATA_TYPE
CLASS_USER_HASH_TABLE :: Find ( KEY_TYPE key ) const
{
  typedef HASH_ELEMENT < KEY_TYPE, DATA_TYPE > *pHASH_ELEMENT;
  HASH hash = _hash(key) % _num_elements;
  pHASH_ELEMENT hash_element = _data[hash];

  // fprintf ( TFile, "Find: 0x%08lx %2d 0x%08lx %s\n",
  //	      key, hash, hash_element, key );
  // fflush ( TFile );

  for ( ; hash_element != NULL; hash_element = hash_element->_next ) {
    if ( _equal ( hash_element->_signature, key ) ) {
      return ( hash_element->_data );
    }
  }

  return((DATA_TYPE)NULL);
}

TEMPLATE_USER_HASH_TABLE
BOOL
USER_HASH_TABLE_ITER < KEY_TYPE, DATA_TYPE, HASH_FUNC, KEY_EQ > :: Step
  ( KEY_TYPE* key, DATA_TYPE* data )
{
  if ( _he && _he->_next ) {
    _he = _he->_next;
    *key = _he->_signature;
    *data = _he->_data;
    return TRUE;
  }
    
  for ( _loc++; _loc < _hash->Num_Elements(); _loc++ ) {
    if ( _hash->Data(_loc) ) {
      _he = _hash->Data(_loc);
      *key = _he->_signature;
      *data = _he->_data;
      return TRUE;
    }
  }

  return FALSE;
}

#endif
