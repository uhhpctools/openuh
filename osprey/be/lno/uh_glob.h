#ifndef __UH_GLOB_H__
#define __UH_GLOB_H__

#include "wn.h"

typedef enum ParStatus {PAR_AUTO, PAR_MAN, PAR_SYNCH, NOPAR} PAR_STATUS;
typedef struct ParLoopStatus {
  WN* wnLoop;
  double nIterations;
  int iLine;
  bool isIterationSymbolic;

  // parallel status
  std::string sReason;
  PAR_STATUS iStatus;

  // loop cost
  double fOverhead;
  double fMachine;
  double fCache;
  double fCost; // average cost
  double fTotal;

  // data scope of variables
  std::string sPrivate;
  std::string sFirstPrivate;
  std::string sLastPrivate;
  std::string sShared;
  std::string sReduction;
  
  // counter of the data scope
  int iPrivate;
  int iFirstPrivate;
  int iLastPrivate;
  int iShared;
  int iReduction;
} PAR_LOOP_STATUS;


#endif
