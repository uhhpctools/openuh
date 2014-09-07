#ifndef opt_OSA_INCLUDED
#define opt_OSA_INCLUDED        "opt_OSA.h"
#endif

#ifndef opt_bb_INCLUDED
#include "opt_bb.h"
#endif
#ifndef opt_proactive_INCLUDED
#include "opt_proactive.h"
#endif
#ifndef opt_fb_INCLUDED
#include "opt_fb.h"
#endif
#ifndef opt_base_INCLUDED
#include "opt_base.h"
#endif
#ifndef CXX_MEMORY_INCLUDED
#include "cxx_memory.h"
#endif
#ifndef ERRORS_INCLUDED
#include "errors.h"
#endif
#ifndef region_util_INCLUDED
#include "region_util.h"
#endif
//#include <string.h>

class OSAshcall {
  public:
  std::string callname;
  INT64 line;
  OSAshcall() {line=0;callname="";}
};

class OSAedge{
  public:
  INT64 id;  //locally generated def_ids
  INT64 def_id;  //locally generated def_ids
  INT64 use_id; //locally generated use_ids 
  WN *wn_def; //wn for def.
  WN *wn_use; //wn for use.
  WN *wn_parent; //wn for parent of use.
  OSAedge(){};
  ~OSAedge() {};
};


class OSAnode {
  public:
  INT64 id; //local
  INT64 def_id; //0=def,if use then doru==def_id
  char  opcode[20];
  char *var_nm; //could be a register
  INT64 simpid;
  INT64 deleted;
  INT64 line;
  WN *wn_self;
  WN *wn_parent; //will be NULL for def nodes
  vector<INT64> osabarriers;
  vector<OSAshcall> openshmem_calls;
  char *src_file;
  INT64 is_mVal; 
  INT64 is_conditional;
  vector<OSAedge> edges;
  void set_values(INT64 pid, INT64 doru, char (&myopcode)[20], char *varnm, INT64 pline, WN *wn_myself, WN *wn_myparent, char *fnm, INT64 mulV, INT64 is_conditional);
  OSAnode(){};
  ~OSAnode(){};
  INT64 ID() {};
 };

class OSAgraph {
   public:
   vector<OSAnode> nodes;
   void print();
   void printnode(int id);
   std::string getcallcolor(std::string);
   void relabel();
   int CFGIsOpenSHMEM(char *input, int begin, int end);
   OSAgraph() {};
  ~OSAgraph() {nodes.clear();};
 };

