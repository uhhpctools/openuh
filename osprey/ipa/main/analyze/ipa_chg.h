/*
 *
 * Copyright (C) 2006, 2007, Tsinghua University.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.  
 *
 * For further information regarding this notice, see:
 * http://hpc.cs.tsinghua.edu.cn
 *
 */

#ifdef OSP_OPT

#ifndef cxx_ipa_chg_INCLUDED
#define cxx_ipa_chg_INCLUDED

#include "symtab.h"
#include "ipc_symtab_merge.h"
#include <vector>
#include <ext/hash_map>

using __gnu_cxx::hash_map;

class IPA_CLASS_HIERARCHY {

public:

    typedef vector <TY_INDEX> _ty_idx_list;
    typedef hash_map <TY_INDEX, _ty_idx_list> CLASS_RELATIONSHIP; 
	
    IPA_CLASS_HIERARCHY();
    ~IPA_CLASS_HIERARCHY();

    INT Get_Num_Base_Classes(TY_INDEX tyi);
    INT Get_Num_Sub_Classes(TY_INDEX tyi);

    TY_INDEX Get_Base_Class(TY_INDEX tyi, INT index); 
    TY_INDEX Get_Sub_Class(TY_INDEX tyi, INT index);

    void Add_Base_Class(TY_INDEX tyi, TY_INDEX base);
    void Add_Sub_Class(TY_INDEX tyi, TY_INDEX sub);

    BOOL Is_Sub_Class(TY_INDEX tyi, TY_INDEX sub);
    BOOL Is_Ancestor(TY_INDEX ancestor, TY_INDEX descendant); 

private:

    CLASS_RELATIONSHIP baseclass;
    CLASS_RELATIONSHIP subclass;
};

// Global class hierarchy graph
extern IPA_CLASS_HIERARCHY* IPA_Class_Hierarchy;

// Build the global class hierarchy graph
extern IPA_CLASS_HIERARCHY* Build_Class_Hierarchy();

#endif

#endif

