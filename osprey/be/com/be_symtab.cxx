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


#ifdef USE_PCH
#include "be_com_pch.h"
#endif /* USE_PCH */
#pragma hdrstop
#include "be_symtab.h"
#include "pu_info.h"
#include "cxx_memory.h"

BE_SCOPE *Be_scope_tab;

BE_SYMBOL_TABLE Be_symbol_table;

BE_PREG_TAB Be_preg_tab;

static SYMTAB_IDX next_level = 0;

static MEM_POOL Be_symtab_pool;

void
BE_symtab_initialize_be_scopes(void)
{
  MEM_POOL_Initialize(&Be_symtab_pool, "back end symbol table", FALSE);
  MEM_POOL_Push(&Be_symtab_pool);
}

void
BE_symtab_free_be_scopes(void)
{
  MEM_POOL_Pop(&Be_symtab_pool);
  MEM_POOL_Delete(&Be_symtab_pool);
}

void
BE_symtab_alloc_scope_level(const SYMTAB_IDX level)
{
  while (level >= next_level) {
    BE_SCOPE *temp = CXX_NEW_ARRAY(BE_SCOPE, 1 + next_level * 2,
				   &Be_symtab_pool);
    SYMTAB_IDX i;
    for (i = 0; i < next_level; i++) {
      temp[i] = Be_scope_tab[i];
    }
    next_level = 1 + next_level * 2;
    for (; i < next_level; i++) {
      temp[i].be_st_tab = NULL;
    }
    CXX_DELETE_ARRAY(Be_scope_tab, &Be_symtab_pool);
    Be_scope_tab = temp;
  }
  SYMTAB_IDX i = level;
  while (Be_scope_tab[i].be_st_tab == NULL && i > 0) {
    Be_scope_tab[i].be_st_tab = CXX_NEW(BE_ST_TAB, &Be_symtab_pool);
    --i;
  }
}

// Determine if the ST represents a constant
BOOL
ST_is_const_initialized (const ST* st)
{
    /* make sure it's a variable (necessary check?) */
    if ( ST_class(st) != CLASS_VAR )
	return FALSE;

    /* make sure it's a constant */
    if (!ST_is_const_var(st))
	return FALSE;

    // is it a constant with unknown value?
    if (BE_ST_unknown_const(st))
      return FALSE;

    // uninitialized constant is the same as initialized with zero, so we
    // don't check the ST_is_initialized bit

    /* get the type */
    TY_IDX ty = ST_type(st);
    
    /* just because it's constant doesn't mean it can't change behind
     * our backs.
     */
    if (TY_is_volatile(ty)) {
	return FALSE;
    }

    return TRUE;
}


// Support for ST_is_const_initialized_scalar:
struct match_inito_by_st {
private:
  const ST_IDX st_idx;

public:
  match_inito_by_st(const ST *const st) : st_idx(ST_st_idx(st)) { }
  match_inito_by_st(const ST_IDX esstee_idx) : st_idx(esstee_idx) { }

  BOOL operator()(INITO_IDX, const INITO *inito) const
    { return INITO_st_idx(*inito) == st_idx; }
};

// Say whether the specified ST is a constant scalar variable
// initialized by a constant, and if so, copy the TCON for the
// constant into *tcon_copy. The caller takes responsibility for
// entering the TCON into the table if the copy gets modified somehow
// and s/he wants to save the modified version.
BOOL
ST_is_const_initialized_scalar(const ST *st, TCON &tcon_copy)
{
    // Make sure it is not a constant with an unknown value.
    if (BE_ST_unknown_const(st) != 0) {
      Is_True (FALSE, ("Asking for value of unknown const"));
      return FALSE;
    }

    if (!ST_is_const_initialized(st)) 
	return FALSE;

    TY_IDX  ty = ST_type(st);
    TYPE_ID mtype = TY_mtype(ty);

#ifdef TARG_X8664 // bug 10673
    switch (mtype) { // use mtype of vector elements
      case MTYPE_M8I1:
        mtype = MTYPE_I1;
        break;
      case MTYPE_M8I2:
        mtype = MTYPE_I2;
        break;
      case MTYPE_M8I4:
        mtype = MTYPE_I4;
        break;
      default:
        break;
    }
#endif

    // exclude all non-scalars
    if (!Is_Simple_Type(ty)) {
	return FALSE;
    }
    
    // Determine if the symbol is explicitly initialized
    // (the for-loop is necessary to solve f90 bug #626430).
    //
    const ST *base;
    for (base = st; 
	 (!ST_is_initialized(base) && ST_base_idx(base) != ST_st_idx(base));
	 base = ST_base(base));
    
    BOOL initialized = ST_is_initialized(base);
    
    // is the value known to be initialized to zero?
    // uninitialized is equivalent to init. to zero
    //
    if (!initialized || ST_init_value_zero(st)) {
	if (MTYPE_is_integral(mtype)) {
	    tcon_copy = Host_To_Targ(mtype, 0L);
	}
	else {
	    tcon_copy = Host_To_Targ_Float(mtype, 0.0);
	}
	return TRUE;
    }

    // try to find the object that inits us; it must be at the same
    // scope level.
    INITO_IDX inito_idx = For_all_until(Inito_Table,
					ST_IDX_level(ST_st_idx(st)),
					match_inito_by_st(st));

    /* make sure we found it */
    if (inito_idx == (INITO_IDX) 0)
	return FALSE;

    /* make sure we have a value */
    INITV &inov = Initv_Table[INITO_val(inito_idx)];

    switch (INITV_kind(inov)) {
    case INITVKIND_ZERO:
      tcon_copy = Host_To_Targ(mtype, 0L);
      return TRUE;
    case INITVKIND_ONE:
      tcon_copy = Host_To_Targ(mtype, 1L);
      return TRUE;
    case  INITVKIND_VAL:
      tcon_copy = Tcon_Table[INITV_tc(inov)];
      return TRUE;
    }
    return FALSE;
}


extern INITV_IDX
ST_has_initv(const ST *st)
{
  if (!ST_is_initialized (st))
    return (INITV_IDX) 0;

  TY_IDX   ty = ST_type(st);

  // try to find the object that inits us; it must be at the same
  // scope level.
  INITO_IDX inito_idx;
  inito_idx = For_all_until(Inito_Table, ST_IDX_level(ST_st_idx(st)),
			    match_inito_by_st(st));

  if (inito_idx == (INITO_IDX) 0) {
    return (INITV_IDX) 0;
  }
  else {
    return INITO_val(inito_idx);
  }
}


// Determine if the ST represents a constant scalar variable that has
// a known initialized value. If true, returns the INITV_IDX for the
// value.
extern INITV_IDX
ST_is_const_and_has_initv(const ST *st)
{
  // Make sure it is not a constant with an unknown value.
  if (BE_ST_unknown_const(st) != 0) {
    Is_True (FALSE, ("Asking for value/initv of unknown const"));
    return (INITV_IDX) 0;
  }

  if (!ST_is_const_initialized(st))
    return (INITV_IDX) 0;

  return ST_has_initv(st);
}
