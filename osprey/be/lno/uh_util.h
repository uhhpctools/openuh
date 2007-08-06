#ifndef __UH_UTIL__
#define __UH_UTIL__


/* ---------------- Data structure     */
#include <map>
#include "uh_glob.h"
 
/* ---------------- Function definition */
int UH_GetBaseExtName(std::string filename, std::string &filebase);
extern int UH_Node_Dump(WN* wn_node);
extern std::string UH_WNexprToString(WN *wn);
extern void UH_PrintNode(WN *wnRoot);
extern void UH_IPA_IR_Filename_Dirname(SRCPOS srcpos,        /* in */
                                      char *&fname,   /* out */
                                      char *&dirname); /* out */

extern int UH_GetLineNumber(WN *wn);

/* ---------------- Global variable (to avoid) */
extern std::vector<PAR_LOOP_STATUS> vecLoopStatus;
extern std::map<WN*, PAR_LOOP_STATUS> mapLoopStatus;
extern const char *ParStatusStr[];
#endif
