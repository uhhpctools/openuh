/*

  Copyright (C) 2010, Hewlett-Packard Development Company, L.P. All Rights Reserved.

  Open64 is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by 
  the Free Software Foundation; either version 2 of the License, 
  or (at your option) any later version.

  Open64 is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
  MA  02110-1301, USA.

*/


/* ====================================================================
 *
 * Module: comp_driver.h
 *
 * Revision history:
 *  Oct-10 - Original Version
 *
 * Description:
 *  Define the base class to support phase componentization.
 * 
 * Exported classes:
 *  O64_Driver
 *  O64_Option
 *  O64_ComponentInitializer
 *
 * SEE ALSO:
 *  common/util/flags.h (option kind)
 *  common/com/comp_decl.h (O64_Component)
 * 
 * ====================================================================
 */

#ifndef comp_driver_INCLUDED
#define comp_driver_INCLUDED

#include "defs.h"
#include "flags.h"
#include "resource.h"
#include "mempool.h"
#include "errors.h"
#include "wn.h"

struct O64_OptionDescriptor
{
    O64_OptionDescriptor(INT32 id, const char * desc,
        const char * name, const char * abbrev, OPTION_KIND kind,
        OPTION_VISIBILITY visibility, BOOL pragma, INT64 def,
        INT64 min, INT64 max):
        OptionId(id), OptionDesc(desc),
        OptionName(name), OptionAbbrev(abbrev), OptionKind(kind),
        OptionVisibility(visibility), OptionPragma(pragma),
        DefaultVal(def), MinVal(min), MaxVal(max),
        OptionSet(false) { Is_True(kind != OVK_ENUM, ("kind should not be OVK_ENUM")); };

    template <typename enum_type>
    O64_OptionDescriptor(INT32 id, const char * desc,
        const char * name, const char * abbrev,
        OPTION_VISIBILITY visibility, BOOL pragma,
        enum_type def,  enum_type min, enum_type max, enum_type defset,
        const char * const * valuenames):
        OptionId(id), OptionDesc(desc),
        OptionName(name), OptionAbbrev(abbrev), OptionKind(OVK_ENUM),
        OptionVisibility(visibility), OptionPragma(pragma),
        DefaultVal(def), MinVal(min), MaxVal(max),
        DefaultSetVal(defset), ValueNames(valuenames),
        OptionSet(false) {};

    INT32               OptionId;
    OPTION_KIND         OptionKind;
    OPTION_VISIBILITY   OptionVisibility;
    BOOL                OptionPragma; /* options pragma */

    const char *        OptionName;
    const char *        OptionAbbrev;
    const char *        OptionDesc;
    BOOL                OptionSet;  /* the value is set from command line */

    INT64               DefaultVal;
    INT64               MinVal;
    INT64               MaxVal;
    INT64               DefaultSetVal; /* only for OVK_ENUM */
    const char * const* ValueNames;    /* only for OVK_ENUM */
};

struct O64_ComponentDescriptor
{
    const char*                 CompDesc; /* component description */
    const char*                 CompName; /* component name */
    const O64_OptionDescriptor* OptionDescriptors; /* option descriptors */
};

#define O64_OPTION_DESC(id, desc, name, abbrev, kind, visibility, pragma, def, min, max) \
    O64_OptionDescriptor(id, desc, name, abbrev, kind, visibility, pragma, def, min, max)

#define O64_ENUM_OPTION_DESC(id, desc, name, abbrev, visibility, pragma, \
    def, min, max, defset, valuename) \
    O64_OptionDescriptor(id, desc, name, abbrev, visibility, pragma, \
    def, min, max, defset, valuename)

#define O64_COMPONENT_DESC(desc, name, opt_desc) desc, name, opt_desc

struct O64_ComponentDescriptorList
{
    INT32                       NumOfComponents;
    O64_ComponentDescriptor **  ComponentDescriptors;
};

class O64_ComponentInitializer
{
public:
    O64_ComponentInitializer(O64_COMPONENT, const O64_ComponentDescriptor*) ;
};

class O64_Option;               /* forward declaration */

class O64_Driver
{
public:

    enum OPTION     
    {
        OPT_driver_first = OPT_component_first,
        OPT_driver_tstats = OPT_driver_first,
        OPT_driver_mstats,
        OPT_driver_last
    };
    static const O64_ComponentDescriptor ComponentDescriptor;
    static const O64_OptionDescriptor
        OptionDescriptors[OPT_driver_last - OPT_driver_first + 1];

    static O64_Driver*  GetInstance();
    static void         Destroy();

public:

    void                RegisterComponent(O64_COMPONENT, const O64_ComponentDescriptor*);
    O64_Option*         GetCurrentOption () { return _CurrentOption; };
    MEM_POOL *          GetDriverMemPool()  { return &_DriverPool;};        
    MEM_POOL *          GetLocalMemPool()   { return &_LocalPool; };
    WN*                 GetCurrentWN()      { return _CurrentWN; };
    void                SetCurrentWN(WN* wn){ _CurrentWN = wn; };
    bool                ProcessComponentOption(char *argv);
    TRACE_OPTION_KIND   GetTraceKind();
    BOOL                TimeStats();
    BOOL                MemStats();
    
    O64_ComponentDescriptorList* GetComponentDescriptorList() 
        { return &_ComponentDescriptorList; };

private:

    static O64_Driver*          _theInstance;
    
    O64_ComponentDescriptorList _ComponentDescriptorList; /* all component descriptors */
    MEM_POOL                    _DriverPool; /* for data structure across component boundary */
    MEM_POOL                    _LocalPool; /* for each component */
    INT64                       _NumRegisteredComponents;

    O64_Option*                 _CurrentOption;
    WN*                         _CurrentWN;

    O64_Driver();
    ~O64_Driver();

    friend class    O64_Component;
    friend class    O64_Option;

};

struct O64_DriverInitializer
{
    O64_DriverInitializer()   { O64_Driver::GetInstance(); }
    ~O64_DriverInitializer()  { O64_Driver::Destroy();     }
};

// ====================================================================
// Command line O64 option format
//
// command-line-options  ::= component-option[' 'component-option]*
// component-option      ::= '-'component-name':'option-list
// option-list           ::= option['+'option]*
// option                ::= option-name['='option-values]
// option-values         ::= option-value[','option-value]*    
// component-name        ::= ['A'-'Z']+
// option-name           ::= ['a'-'z'|'A'-'Z']+
// option-value          ::= signed integer    | unsigned integer |
//                           boolean | string | enum_value
// boolean               ::= 'TRUE'  | 'FALSE' | 'true' | 'false' |
//                           'ON'    | 'OFF'   | 'on'   | 'off'   |
//                           'YES'   | 'NO'    | 'yes'  | 'no' 
//
// o component names are upper letter 
// o option names are case-insensitive 
// o '-OPTIONS' or '-component-name:OPTIONS' will print out the option
// usage information. 
// =====================================================================

class O64_Option
{
public:

    O64_Option();
    ~O64_Option();

    BOOL            GetBoolOption(INT32 component, INT32 option);
    INT64           GetIntOption(INT32 component, INT32 option);
    UINT64          GetUIntOption(INT32 component, INT32 option);
    char *          GetStringOption(INT32 component, INT32 option);
    OPTION_LIST *   GetListOption(INT32 component, INT32 option);
    template <typename enum_ret>
    enum_ret        GetEnumOption(INT32 component, INT32 option);

private:

    union _Value
    {
        BOOL _BoolVal;
        INT64 _IntVal;
        UINT64 _UIntVal;
        char * _StringVal;
        OPTION_LIST * _ListVal;
        INT32 _EnumVal;
     };

    INT32                           GetNumOfComponents_() const;
    const O64_ComponentDescriptor*  GetComponentDescriptor_(INT32 component);
    const O64_OptionDescriptor *    GetOptionDescriptor_(INT32 component, INT32 option);
    
    const char *        GetComponentName_(INT32 component);
    const char *        GetOptionName_ (INT32 component, INT32 option);
    const char *        GetOptionAbbrev_(INT32 component, INT32 option);
    const char *        GetOptionDesc_(INT32 component, INT32 option);
    const OPTION_KIND   GetOptionKind_(INT32 component, INT32 option);     
    INT64               GetDefaultVal_(INT32 component, INT32 option);
    INT64               GetMinVal_(INT32 component, INT32 option);
    INT64               GetMaxVal_(INT32 component, INT32 option);
    BOOL                GetOptionSet_(INT32 component, INT32 option);
    const char * const* GetValueNames_(INT32 component, INT32 option);    
    INT64               GetDefaultSetVal_(INT32 component, INT32 option);
    void                SetDefaultOption_(INT32 component, INT32 option);

    bool                ProcessComponentOption_(char * options);
    bool                ProcessOptionList_(INT32 component, char* options);
    bool                ProcessOption_(INT32 component, char *value); 
    bool                SetOptionValue_(INT32 component, INT32 option, char *value);
    void                PrintOptionsUsage_();
    void                PrintOptionsUsage_(INT32 component);

private:

    _Value **           _Options; /* store value for each option for each component */
    MEM_POOL *          _MemPool; /* point to O64_Driver's _DriverPool */
    
    const O64_OptionDescriptor*         _CommonOptionDescriptors;
    const O64_ComponentDescriptorList * _ComponentDescriptorList; /* to component descriptor list */

    friend class    O64_Driver;
    friend class    O64_Component;
     
};

// =======================================================================
// O64_Option inline function implementations
// =======================================================================

inline const O64_ComponentDescriptor*
O64_Option::GetComponentDescriptor_(INT32 component)
{
    return _ComponentDescriptorList->ComponentDescriptors[component]; 
}

inline const char *
O64_Option::GetComponentName_(INT32 component)
{
    if (component == GetNumOfComponents_())
         return "COMMON";
    else
        return GetComponentDescriptor_(component)->CompName;
}

inline BOOL
O64_Option::GetBoolOption(INT32 component, INT32 option)
{
    OPTION_KIND kind = GetOptionKind_(component, option);
    Is_True(kind == OVK_NONE || kind == OVK_BOOL,
            ("GetBoolOption invalid option"));
    return _Options[component][option]._BoolVal;
}

template <typename enum_type>
inline enum_type
O64_Option::GetEnumOption(INT32 component, INT32 option)
{
    OPTION_KIND kind = GetOptionKind_(component, option);
    Is_True(kind == OVK_ENUM, ("GetEnumOption invalid option"));
    return enum_type (_Options[component][option]._EnumVal);
}

inline INT64
O64_Option::GetIntOption(INT32 component, INT32 option)
{
    OPTION_KIND kind = GetOptionKind_(component, option);
    Is_True(kind == OVK_INT32 || kind == OVK_INT64,
        ("GetIntOption invalid option"));
    return _Options[component][option]._IntVal;
}

inline UINT64
O64_Option::GetUIntOption(INT32 component, INT32 option)
{
    OPTION_KIND kind = GetOptionKind_(component, option);
    Is_True(kind == OVK_UINT32 || kind == OVK_UINT64,
        ("GetUIntOption invalid option"));
    return _Options[component][option]._UIntVal;
}

inline char *
O64_Option::GetStringOption(INT32 component, INT32 option)
{
    OPTION_KIND kind = GetOptionKind_(component, option);
    Is_True(kind == OVK_NAME || kind == OVK_SELF, 
        ("GetStingOption invalid option"));
    return _Options[component][option]._StringVal;
}

inline OPTION_LIST *
O64_Option::GetListOption(INT32 component, INT32 option)
{
    OPTION_KIND kind = GetOptionKind_(component, option);
    Is_True(kind == OVK_LIST, ("GetListOption invalid option"));
    return _Options[component][option]._ListVal;
}

inline const OPTION_KIND
O64_Option::GetOptionKind_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component,option)->OptionKind;
}

inline const O64_OptionDescriptor *
O64_Option::GetOptionDescriptor_(INT32 component, INT32 option)
{
    // The common options are from _CommonOptionDescriptors
    if ((option < OPT_common_last ) || (option == OPT_common_last) 
        && (component == GetNumOfComponents_()))
       return &_CommonOptionDescriptors[option];
    else
        return 
            &GetComponentDescriptor_(component)->OptionDescriptors[option - OPT_common_last];
}

inline INT32 
O64_Option::GetNumOfComponents_() const
{
    return _ComponentDescriptorList->NumOfComponents;
}

inline void
O64_Option::SetDefaultOption_(INT32 component, INT32 option)
{
    switch (GetOptionKind_(component, option))
    {
        case OVK_NONE:
        case OVK_BOOL:
            _Options[component][option]._BoolVal=
                (UINT32) GetDefaultVal_(component, option);
            break;

        case OVK_INT32:
            _Options[component][option]._IntVal=
                (INT32) GetDefaultVal_(component, option);
            break;

        case OVK_INT64:
            _Options[component][option]._IntVal =
                GetDefaultVal_(component, option);

        case OVK_UINT32:
            _Options[component][option]._UIntVal =
                (UINT32) GetDefaultVal_(component, option);
            break;
    
        case OVK_UINT64:
            _Options[component][option]._UIntVal =
                (UINT64) GetDefaultVal_(component, option);
            break;

        case OVK_NAME:
        case OVK_SELF:
            _Options[component][option]._StringVal = NULL;
            break;

        case OVK_LIST:
            _Options[component][option]._ListVal = NULL;
            break;
            
        case OVK_ENUM:
            _Options[component][option]._EnumVal = 
                (INT32) GetDefaultVal_(component, option);
            break;
        default:
            Is_True(false, ("Unimplemented option kind"));
            break;
    } 
};

inline INT64
O64_Option::GetDefaultVal_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->DefaultVal;
}

inline const char *
O64_Option::GetOptionName_ (INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->OptionName;
}

inline BOOL
O64_Option::GetOptionSet_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->OptionSet;
}

inline INT64 
O64_Option::GetMinVal_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->MinVal;
}

inline INT64
O64_Option::GetMaxVal_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->MaxVal;
}


inline const char*
O64_Option::GetOptionAbbrev_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->OptionAbbrev;

}

inline const char *
O64_Option::GetOptionDesc_(INT32 component, INT32 option)
{
    return GetOptionDescriptor_(component, option)->OptionDesc;
}    

inline const char * const *
O64_Option::GetValueNames_(INT32 component, INT32 option)
{
    Is_True(GetOptionKind_(component, option) == OVK_ENUM, ("should be OVK_ENUM"));    
    return GetOptionDescriptor_(component, option)->ValueNames;
}

inline INT64
O64_Option::GetDefaultSetVal_(INT32 component, INT32 option)
{
    Is_True(GetOptionKind_(component, option) == OVK_ENUM, ("should be OVK_ENUM"));
    return GetOptionDescriptor_(component, option)->DefaultSetVal;
}
    
#endif /* comp_driver_INCLUDED */
