//-*-c++-*-

/*
 * Copyright 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

// ====================================================================
// ====================================================================
//
// Module: opt_ssu.h
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/opt/opt_ssu.h,v $
//
// Revision history:
//  11-DEC-96 ptu - Original Version
//
// ====================================================================
//
// Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it would be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Further, this software is distributed without any warranty that it
// is free of the rightful claim of any third person regarding
// infringement  or the like.  Any license provided herein, whether
// implied or otherwise, applies only to this software file.  Patent
// licenses, if any, provided herein do not apply to combinations of
// this program with other software, or any other product whatsoever.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
//
// Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
// Mountain View, CA 94043, or:
//
// http://www.sgi.com
//
// For further information regarding this notice, see:
//
// http://oss.sgi.com/projects/GenInfo/NoticeExplan
//
// ====================================================================
//
// Description:
//
//   IPHI_HASH_ENTRY:the individual node that carries the iphi node
//                   in the iphi-hash table.
//            the body of the class contains:
//
//     _bb:          which BB this iphi function is associated with
//     _iphi_result: this is actually a flag, tells whether this iphi
//                   node is a variable or an expression by checking
//                   the Kind().
//     _node:        contains the real iphi node.  TODO: This field
//                   can be deleted if we do not find use of it.
//
// ====================================================================
// ====================================================================


#ifndef opt_ssu_INCLUDED
#define opt_ssu_INCLUDED	"opt_ssu.h"

#include "defs.h"
#ifndef errors_INCLUDE
#include "errors.h"
#endif
#include "opt_defs.h"
#include "cxx_memory.h"
#include "opt_sym.h"

//  Forward declaration
class CFG;
class OPT_STAB;
class BB_NODE;
class BB_LIST;
class STMTREP;
class IPHI_LIST;
class COPYPROP;
class EXP_OCCURS;
class ETABLE;

class SSU {
private:
  MEM_POOL *_mem_pool;
  MEM_POOL *_loc_pool;  
  CFG      *_cfg;
  CODEMAP  *_htable;
  OPT_STAB *_opt_stab;
  ETABLE   *_etable;
  BOOL      _tracing;
  IDX_32_SET *_e_num_set;
  IDX_32_SET **_make_diff_ssu_version_called_in_bb; // array indexed
  		// by BB id of set of aux_id's processed already by 
		// Make_diff_ssu_version

            SSU(const SSU&);
            SSU& operator = (const SSU&);

  OPT_STAB *Opt_stab(void) const { return _opt_stab; }
  CFG      *Cfg(void)      const { return _cfg; }
  CODEMAP  *Htable(void)   const { return _htable; }
  ETABLE   *Etable(void)   const { return _etable; }
  BOOL      Tracing(void)  const { return _tracing; }

  EXP_WORKLST *SPRE_candidate(CODEREP *cr);
  void	    Insert_iphis_recursive(EXP_WORKLST *, BB_NODE *);
  void	    Make_non_postdominated_iphi_opnd_null(BB_NODE *iphibb, 
						  EXP_PHI *iphi);
  BOOL	    Find_intervening_iphi(EXP_WORKLST *wk, CODEREP *v, BB_NODE *usebb);
  void	    Make_diff_ssu_version_at_phi(EXP_WORKLST *wk,
					 BB_NODE *defbb,
					 PHI_NODE *phi);
  void	    Check_iphi_presence(EXP_WORKLST *wk,
  				BB_NODE *iphibb);
  void	    Make_null_ssu_version_in_iphi_for_e_num_set(
					  BB_NODE *iphibb,
					  BB_NODE *usebb);
  void	    Make_diff_ssu_version(EXP_WORKLST *wk, 
				  CODEREP *v, 
				  BB_NODE *usebb,
				  BOOL only_itself);
  void      Traverse_mu_read(MU_LIST *, BB_NODE *);
  void      Traverse_cr_rw(CODEREP *, BB_NODE *, BOOL is_store);
  void      Iphi_insertion(void);
  inline void Reset_tos_downsafe(void);
  void	    Propagate_occurrences(EXP_OCCURS *iphi_occ, CODEREP *cr);
  void      Rename(BB_NODE *bb);

public:
            SSU(void);
            SSU(CODEMAP  *htable,
		CFG      *cfg, 
		OPT_STAB *opt_stab, 
		ETABLE   *etable,
		MEM_POOL *gpool,
		MEM_POOL *lpool,
                BOOL      tracing) 
	                         { _htable   = htable;
				   _cfg      = cfg;
				   _opt_stab = opt_stab;
				   _etable   = etable;
				   _mem_pool = gpool; 
				   _loc_pool = lpool;
                                   _tracing  = tracing;
				   _e_num_set = CXX_NEW(IDX_32_SET(etable->Exp_worklst()->Len(), 
				   				   lpool, 
								   OPTS_DONT_CARE), 
						       lpool);
				  _make_diff_ssu_version_called_in_bb =
				    (IDX_32_SET **) CXX_NEW_ARRAY(IDX_32_SET *,
						cfg->Total_bb_count(), lpool);
				  for (INT32 i = 0; i < cfg->Total_bb_count();
				       i++)
				    _make_diff_ssu_version_called_in_bb[i] = 
				   	CXX_NEW(IDX_32_SET(opt_stab->Lastidx(), 
							   lpool, 
							   OPTS_FALSE),
						lpool); 
                                 }
           ~SSU(void) 	  { /*CXX_DELETE_ARRAY(_make_diff_ssu_version_called_in_bb, lpool);*/ }

  void      Construct(void);
  MEM_POOL *Mem_pool(void) const { return _mem_pool; }
  MEM_POOL *Loc_pool(void) const { return _loc_pool; }
};

#if 0
class CODEREP;

// Iphi functions
class IPHI_NODE : public SLIST_NODE {
  DECLARE_SLIST_NODE_CLASS(IPHI_NODE)
private:

  enum IPHI_NODE_FLAGS {
    IPNF_NONE		= 0x00,
  };

  AUX_ID      _aux_id;
  mUINT8      _flags;
  INT16       _size;
  INT16       _count;
  BB_NODE    *_bb;

  union IPHI_ELEM {
  friend class IPHI_NODE;
  private:
    EXP_OCCURS *_occ;
  };

  // vec[0] is the result
  // vec[1..] are the operands
  IPHI_ELEM *_vec;

  IPHI_NODE(void);
  IPHI_NODE(const IPHI_NODE&);
  IPHI_NODE& operator = (const IPHI_NODE&);

public:

  IPHI_NODE(INT16 in_degree, MEM_POOL *pool, BB_NODE *bb)
    { _vec = (IPHI_ELEM*) CXX_NEW_ARRAY(IPHI_ELEM, (in_degree + 1), pool);
      _size = in_degree; _count = 0; _bb = bb; _flags = IPNF_NONE; }
  ~IPHI_NODE(void)                  
    { /*CXX_DELETE_ARRAY(_vec, Opt_default_pool);*/ }
  
  EXP_OCCURS *Opnd(INT32 i) const           { return _vec[i+1]._occ; }
  EXP_OCCURS *Result(void) const            { return _vec[0]._occ; }
  INT16       Size(void) const              { return _size; }
  INT16       Count(void) const             { return _count; }
  IDTYPE      Aux_id(void) const            { return _aux_id; }
  BB_NODE    *Bb(void) const                { return _bb; }
  
  mUINT8      Flags(void) const             { return _flags; }
  void        Set_flags(mUINT8 i)           { _flags = i; }
  void        Set_aux_id(IDTYPE id)         { _aux_id = id; }
  void        Set_result(EXP_OCCURS *occ)   { _vec[0]._occ = occ; }
  void        Set_count(INT16 c)            { _count = c; }
  void        Set_opnd(const INT32 i, 
		       EXP_OCCURS *occ)     { _vec[i+1]._occ = occ; }

  // Remove the i'th operand from the iphi-node (0 is first opnd)
  void        Remove_opnd(INT32 i);         

  // flags field access functions

  //  Print functions
  void        Print(INT32 in_degree, FILE *fp=stderr) const;
  void        PRINT(INT32 in_degree, FILE *fp=stderr) const;
};

class IPHI_LIST : public SLIST {
  DECLARE_SLIST_CLASS (IPHI_LIST, IPHI_NODE)

private:
  INT32   in_degree;          

  IPHI_LIST(const IPHI_LIST&);
  IPHI_LIST& operator = (const IPHI_LIST&);

public:
  IPHI_LIST(BB_NODE *bb);
  ~IPHI_LIST(void)			 {}

  IPHI_NODE *New_iphi_node(IDTYPE var, MEM_POOL *pool, BB_NODE *bb)
    { 
      IPHI_NODE *p = 
	(IPHI_NODE *) CXX_NEW(IPHI_NODE(in_degree, pool, bb), pool); 
      for (INT32 i = 0; i < in_degree; i++)
	p->Set_opnd(i, NULL);
      p->Set_result(NULL);
      p->Set_aux_id(var);
      Append(p);
      return p;
    }

  IPHI_LIST *Dup_iphi_node(MEM_POOL *pool, BB_NODE *bb, INT pos);
  // Remove the i'th operand from the iphi-nodes (0 is first opnd)
  void     Remove_opnd(INT32 i);
	
  INT32 In_degree(void) const           { return in_degree; }
  void  Set_in_degree( INT32 n )	{ in_degree = n; }

  void  Print(FILE *fp=stderr);    
  void  PRINT(FILE *fp=stderr);    
};


class IPHI_LIST_ITER : public SLIST_ITER {
private:
  DECLARE_SLIST_ITER_CLASS (IPHI_LIST_ITER, IPHI_NODE, IPHI_LIST)

  IPHI_LIST_ITER(const IPHI_LIST_ITER&);
  IPHI_LIST_ITER& operator = (const IPHI_LIST_ITER&);

public:
  ~IPHI_LIST_ITER(void)		{}

  IPHI_NODE *First_elem(void)	{ return First(); }
  IPHI_NODE *Next_elem(void)	{ return Next();  }
};

class IPHI_OPND_ITER {
private:
  IPHI_NODE *_iphi;
  INT       _curidx;

  IPHI_OPND_ITER(void);
  IPHI_OPND_ITER(const IPHI_OPND_ITER&);
  IPHI_OPND_ITER& operator = (const IPHI_OPND_ITER&);

public:
  IPHI_OPND_ITER(IPHI_NODE *iphi)   { _iphi = iphi; }
  ~IPHI_OPND_ITER(void)             {}
  void        Init(IPHI_NODE *iphi) { _iphi = iphi; }
  void        Init(void)            {}
  EXP_OCCURS *First_elem(void)      { _curidx = 0; 
				      return _iphi->Opnd(_curidx); }
  EXP_OCCURS *Next_elem(void)       { _curidx++; return 
					(Is_Empty() ? NULL :
					              _iphi->Opnd(_curidx)); }
  BOOL        Is_Empty(void) const  { return _curidx >= _iphi->Size(); }
  INT	      Curidx(void) const    { return _curidx; }
};

// ========================================================================
// Currently, I don't see the need for hash mechanism for IPHI because
// the renaming phase scans the whole program and keeps track of current
// versions of all variables.  
// Just in case that we will need hash, the framework stays here.
// ========================================================================
class IPHI_HASH_ENTRY : public SLIST_NODE {
  DECLARE_SLIST_NODE_CLASS(IPHI_HASH_ENTRY)

private:
  const BB_NODE    *_bb;
        IPHI_NODE  *_var_iphi;

	IPHI_HASH_ENTRY(const IPHI_HASH_ENTRY&);
	IPHI_HASH_ENTRY& operator = (const IPHI_HASH_ENTRY&);

public:
  IPHI_HASH_ENTRY(void)                     {}
  IPHI_HASH_ENTRY(const BB_NODE  *bb, IPHI_NODE *var_iphi);
  ~IPHI_HASH_ENTRY();

  // 'this' contains the 'cr' in the same bb
  BOOL       Is_the_same_as(const AUX_ID aux_id, const BB_NODE *bb);
  IPHI_NODE *Var_iphi(void) const      { return _var_iphi; }
};

class IPHI_HASH_ENTRY_CONTAINER : public SLIST {
private:
  IPHI_HASH_ENTRY_CONTAINER(const IPHI_HASH_ENTRY_CONTAINER&);
  IPHI_HASH_ENTRY_CONTAINER& operator = (const IPHI_HASH_ENTRY_CONTAINER&);
  DECLARE_SLIST_CLASS( IPHI_HASH_ENTRY_CONTAINER, IPHI_HASH_ENTRY )

public:  
  ~IPHI_HASH_ENTRY_CONTAINER(void)       {};

  IPHI_HASH_ENTRY *Find_iphi_hash_entry(const AUX_ID aux_id, const BB_NODE *bb);

  void     Append (IPHI_HASH_ENTRY *hash_entry,
                   IDX_32           idx,
                   CODEMAP         *htable);
};

class IPHI_HASH_ENTRY_ITER : public SLIST_ITER {
  DECLARE_SLIST_ITER_CLASS(IPHI_HASH_ENTRY_ITER,
                           IPHI_HASH_ENTRY,
                           IPHI_HASH_ENTRY_CONTAINER )
public:
  void     Init(void)                    {}
};

#endif
#endif  // opt_ssu_INCLUDED
