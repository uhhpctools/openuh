/*
 Copyright (C) 2010, Hewlett-Packard Development Company, L.P.
 All Rights Reserved.

 Open64 is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 Open64 is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 MA  02110-1301, USA.
*/
#include <elf.h>
#include <stack>

#include "defs.h"
#include "config_ipa.h"
#include "constraint_graph_escanal.h"
#include "constraint_graph_solve.h"
#include "cse_table.h"
#include "defs.h"
#include "ipl_summary.h"
#include "ipa_summary.h"
#include "ipa_be_summary.h"
#include "ipa_nystrom_alias_analyzer.h"
#include "opt_defs.h"
#include "wn_util.h"
#include "ir_reader.h"
#include "ipa_chg.h"
#include "demangle.h"

IPA_NystromAliasAnalyzer *IPA_NystromAliasAnalyzer::_ipa_naa = NULL;

// Performs cycle detection within the call graph.  The
// algorithm employed is Nuutila's algorithm:
//
// Esko Nuutila and Eljas Soisalon-Soininen, "On finding the
//   strongly connected components in a directed graph".
//   Inf. Process. Letters, 49(1):9-14, 1994.
//
// The algorithm provides two benefits.  First it is an improvement
// over Tarjan's algorithm, runs in O(E) time.  Second it produces
// a topological ordering of the target graph for free, which
// will improve the efficiency of the constraint graph solver.
//
// The implementation is derived from the pseudo code in the
// following paper:
//
// Pereira and Berlin, "Wave Propagation and Deep Propagation for
//   Pointer Analysis", CGO 2009, 126-135, 2009.
//

class IPASCCDetection {
public:
  typedef stack<IPA_NODE *> IPANodeStack;

  IPASCCDetection(IPA_NystromAliasAnalyzer *nyst, IPA_CALL_GRAPH *icg,
                  MEM_POOL *mpool)
  : _nyst(nyst),
    _graph(icg),
    _memPool(mpool),
    _I(0),
    _D(NULL)
  {}

  // Detect and unify all SCCS within the call graph.
  void findAndUnify();

  // Return a handle to the stack of nodes in topological
  // order.  This will be used to seed the initial solution
  // and improve efficiency.
  IPANodeStack &topoNodeStack() { return _T; }

private:

  void visit(IPA_NODE *node);

  void find(void);
  void unify(void);

  IPA_NystromAliasAnalyzer *_nyst;
  IPA_CALL_GRAPH           *_graph;
  MEM_POOL                 *_memPool;
  UINT32                   _I;
  UINT32                  *_D;
  IPANodeStack             _S;
  IPANodeStack             _T;
};

void
IPASCCDetection::visit(IPA_NODE *v)
{
  if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
    fprintf(stderr,"visit: IPA_NODE %d, %s\n",v->Node_Index(),v->Name());
  _I += 1;
  _D[v->Node_Index()] = _I;
  _nyst->rep(v,v);
  _nyst->setVisited(v,true);
  IPA_SUCC_ITER succIter(_graph,v);
  for (succIter.First(); !succIter.Is_Empty(); succIter.Next())
  {
    IPA_EDGE *edge = succIter.Current_Edge();
    IPA_NODE *w = _graph->Callee(edge);
    if (!_nyst->visited(w))
      visit(w);
    if (!_nyst->inSCC(w))
    {
      IPA_NODE *rep;
      rep = _D[_nyst->rep(v)->Node_Index()] < _D[_nyst->rep(w)->Node_Index()] ?
          _nyst->rep(v) : _nyst->rep(w);
      _nyst->rep(v,rep);
    }
  }
  if (_nyst->rep(v) == v) {
    _nyst->setInSCC(v,true);
    while (!_S.empty()) {
      IPA_NODE *w = _S.top();
      if (_D[w->Node_Index()] <= _D[v->Node_Index()])
        break;
      else {
        _S.pop();
        _nyst->inSCC(w);
        _nyst->rep(w,v);
      }
    }
    _T.push(v);
  }
  else
    _S.push(v);
}

void
IPASCCDetection::find(void)
{
  // Visit each unvisited root node.   A root node is defined
  // to be a node that has no incoming copy/skew edges
  IPA_NODE_ITER nodeIter(_graph,DONTCARE);
  for (nodeIter.First(); !nodeIter.Is_Empty(); nodeIter.Next()) {
    IPA_NODE *node = nodeIter.Current();
    if (!node) continue;
    if (!_nyst->visited(node)) {
      // We skip any nodes that have a representative other than
      // themselves.  Such nodes occur as a result of merging
      // nodes either through unifying an ACC or other node
      // merging optimizations.  Any such node should have no
      // outgoing edges and therefore should no longer be a member
      // of an SCC.
      if (_nyst->rep(node) == NULL || _nyst->rep(node) == node)
        visit(node);
      else
        _nyst->visited(node);
    }
  }
}

void
IPASCCDetection::unify()
{
  // Unify the nodes in an SCC into a single node
  IPA_NODE_ITER nodeIter(_graph,DONTCARE);
  for (nodeIter.First(); !nodeIter.Is_Empty(); nodeIter.Next()) {
    IPA_NODE *node = nodeIter.Current();
    if (!node) continue;
    FmtAssert(_nyst->visited(node),
        ("Node %d unvisited during SCC detection\n",node->Node_Index()));
    _nyst->setVisited(node,false);
    _nyst->setInSCC(node,false);
    IPA_NODE *rep = _nyst->rep(node);
    if (rep && rep != node) {
      if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
        fprintf(stderr,"Unify: IPA_NODE %d,%s -> IPA_NODE %d,%s\n",
              node->Node_Index(),node->Name(),
              rep->Node_Index(),node->Name());
      // TODO:  Actually merge constraint graphs for these nodes
      ConstraintGraph *nodeCG = _nyst->cg(node->Node_Index());
      ConstraintGraph *repCG =  _nyst->cg(rep->Node_Index());
      //repCG->merge(nodeCG);
    }
  }
}

void
IPASCCDetection::findAndUnify()
{
  // Reset state
  _I = 0;
  _D = CXX_NEW_ARRAY(UINT32,_graph->Node_Size(),_memPool);
  while (!_S.empty()) _S.pop();
  while (!_T.empty()) _T.pop();

  // Make sure that our aux call graph info has been set up.
  _nyst->allocSCCInfo(_graph);

  find();
  unify();

  CXX_DELETE_ARRAY(_D,_memPool);
}

CG_ST_IDX 
ConstraintGraph::adjustCGstIdx(IPA_NODE *ipaNode, CG_ST_IDX cg_st_idx)
{
  UINT16 fileIdx = (UINT16)(ipaNode->File_Index());
  UINT16 puIdx   = (UINT16)(ipaNode->Proc_Info_Index());
  UINT32 filePUIdx = (fileIdx << 16) | puIdx;
  cg_st_idx = (((UINT64)filePUIdx) << 32) | (cg_st_idx & 0x00000000ffffffffLL);
  return cg_st_idx;
}

void
IPA_NystromAliasAnalyzer::buildIPAConstraintGraph(IPA_NODE *ipaNode)
{
  ConstraintGraph *cg = CXX_NEW(ConstraintGraph(&_memPool, ipaNode), &_memPool);
  _ipaConstraintGraphs[ipaNode->Node_Index()] = cg;
}

void 
IPA_NystromAliasAnalyzer::print(FILE *file)
{
  fprintf(file, "\nPrinting globalConstraintGraph...\n");
  ConstraintGraph::globalCG()->print(file);
  IPACGMapIterator iter = _ipaConstraintGraphs.begin();
  for (; iter != _ipaConstraintGraphs.end(); iter++) {
    ConstraintGraph *cg = iter->second;
    fprintf(file, "\nPrinting local ConstraintGraph...\n");
    fprintf(file, "IPA node: proc: %s file: %s\n", 
            cg->ipaNode()->Name(), cg->ipaNode()->File_Header().file_name);
    cg->print(file);
  }
  char buf[32];
  sprintf(buf,"ipa_initial");
  ConstraintGraphVCG::dumpVCG(buf);
}

ConstraintGraphNode *
ConstraintGraph::findUniqueNode(CGNodeId cgNodeId)
{
  CGIdToNodeMapIterator iter = _uniqueCGNodeIdMap.find(cgNodeId);
  FmtAssert(iter != _uniqueCGNodeIdMap.end(), ("Unique id not found"));
  return iter->second;
}

CallSite *
ConstraintGraph::findUniqueCallSite(CallSiteId csid)
{
  CallSiteIterator iter = _uniqueCallSiteIdMap.find(csid);
  FmtAssert(iter != _uniqueCallSiteIdMap.end(), ("Unique cs id not found"));
  return iter->second;
}

void 
ConstraintGraph::buildCGipa(IPA_NODE *ipaNode)
{
  INT32 size;
  SUMMARY_PROCEDURE *proc = ipaNode->Summary_Proc();

  if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
    fprintf(stderr, "Processing proc: %s(%d) file: %s(%d)\n",
            ipaNode->Name(), ipaNode->Proc_Info_Index(),
            ipaNode->File_Header().file_name, ipaNode->File_Index());

  sprintf(_name, "%s(%d),%s(%d)", 
          ipaNode->File_Header().file_name, ipaNode->File_Index(),
          ipaNode->Name(), ipaNode->Proc_Info_Index());
  FmtAssert(strlen(_name)<1024,("Procedure name too long..."));

  // during summary to constraint graph construction, map them to their 
  // globally unique ids so that lookups don't fail
  _uniqueCGNodeIdMap[notAPointer()->id()] = notAPointer();

  // Add the StInfos.
  UINT32 stInfoIdx = proc->Get_constraint_graph_stinfos_idx();
  UINT32 stInfoCount = proc->Get_constraint_graph_stinfos_count();
  SUMMARY_CONSTRAINT_GRAPH_STINFO *summStInfos = 
          IPA_get_constraint_graph_stinfos_array(ipaNode->File_Header(), size);
  FmtAssert(stInfoCount <= size, ("Invalid stInfoCount"));
  for (UINT32 i = 0; i < stInfoCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_STINFO &summStInfo = summStInfos[stInfoIdx + i];
    // Ignore notAPointer; they have been already
    // added to the globalConstraintGraph
    if (summStInfo.firstOffset() == notAPointer()->id())
      continue;
    StInfo *stInfo;
    // Determine which CG to add the StInfo
    if (summStInfo.flags() & CG_ST_FLAGS_GLOBAL)
      stInfo = globalCG()->buildStInfo(&summStInfo, ipaNode);
    else
      stInfo = buildStInfo(&summStInfo, ipaNode);
    _ipaCGStIdxToStInfoMap[summStInfo.cg_st_idx()] = stInfo;
  }

  // Add the ConstraintGraphNodes
  UINT32 nodeIdx = proc->Get_constraint_graph_nodes_idx();
  UINT32 nodeCount = proc->Get_constraint_graph_nodes_count();
  SUMMARY_CONSTRAINT_GRAPH_NODE *summNodes = 
          IPA_get_constraint_graph_nodes_array(ipaNode->File_Header(), size);
  FmtAssert(nodeCount <= size, ("Invalid nodeCount"));
  for (UINT32 i = 0; i < nodeCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_NODE &summNode = summNodes[nodeIdx + i];
    // Ignore notAPointer CGNodes; they have been already
    // added to the globalConstraintGraph
    if (summNode.cgNodeId() == notAPointer()->id())
      continue;
    ConstraintGraphNode *cgNode;
    // Determine which CG to add the ConstraintGraphNode
    StInfo *stInfo = 
            _ipaCGStIdxToStInfoMap.find(summNode.cg_st_idx())->second;
    if (stInfo->checkFlags(CG_ST_FLAGS_GLOBAL))
      cgNode = globalCG()->buildCGNode(&summNode, ipaNode);
    else
      cgNode = buildCGNode(&summNode, ipaNode);
    _uniqueCGNodeIdMap[summNode.cgNodeId()] = cgNode;
  }

  // Set the firstOffset on the StInfo
  for (UINT32 i = 0; i < stInfoCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_STINFO &summStInfo = summStInfos[stInfoIdx + i];
    // Ignore notAPointer; they have been already
    // added to the globalConstraintGraph
    if (summStInfo.firstOffset() == notAPointer()->id())
      continue;
    UINT32 firstOffsetId = summStInfo.firstOffset();
    if (firstOffsetId != 0) {
      StInfo *stInfo = 
              _ipaCGStIdxToStInfoMap.find(summStInfo.cg_st_idx())->second;
      FmtAssert(!stInfo->checkFlags(CG_ST_FLAGS_PREG),
                ("PREGs should have no nextOffset"));
      ConstraintGraphNode *firstOffset = findUniqueNode(firstOffsetId);
      bool added = addCGNodeInSortedOrder(stInfo, firstOffset);
      if (added)
        stInfo->incrNumOffsets();
    }
  }

  // Node ids array
  UINT32 nodeIdsIdx = proc->Get_constraint_graph_node_ids_idx();
  UINT32 nodeIdsCount = proc->Get_constraint_graph_node_ids_count();
  UINT32 *nodeIds = 
          IPA_get_constraint_graph_node_ids_array(ipaNode->File_Header(), size);
  FmtAssert(nodeIdsCount <= size, ("Invalid nodeIdsCount"));

  // Update parents of node provided it does not have an existing parent
  for (UINT32 i = 0; i < nodeCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_NODE &summNode = summNodes[nodeIdx + i];
    // Ignore notAPointer CGNodes; they have been already
    if (summNode.cgNodeId() == notAPointer()->id())
      continue;
    ConstraintGraphNode *cgNode = findUniqueNode(summNode.cgNodeId());
    UINT32 repParentId = summNode.repParent();
    // If the node does not have an existing parent 
    if (repParentId != 0 && cgNode->repParent() == NULL) {
      ConstraintGraphNode *repPNode = findUniqueNode(repParentId);
      ConstraintGraphNode *repPNodeParent = repPNode->findRep();
      // Here we are going to call merge to merge 'cgNode' into
      // the equivalent of the parent.  Why?  It may be the case,
      // that 'summNode.cgNodeId()' was mapped to an existing node
      // having incoming edges.  Now that we are going to give it a
      // representative we must call merge() to ensure it has no
      // incoming/outgoing edges, i.e. they are all mapped onto the
      // parent.  When we read the edges for this PU (below) we will
      // materialize the 'repPNode' -- =0 --> 'cgNode' parent copy.
      if (repPNodeParent != cgNode) {
        repPNodeParent->merge(cgNode);
        cgNode->repParent(repPNodeParent);
      }
    }

    // Set the collapsed parent
    if (cgNode->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
      if (summNode.collapsedParent() != 0) {
        ConstraintGraphNode *cp = findUniqueNode(summNode.collapsedParent());
        //FmtAssert(cgNode->collapsedParent() == 0 ||
        //          cgNode->collapsedParent() == cp->id(),
        //          ("Attempting to collapse node: %d that has an existing "
        //           "collapsed parent: %d to new collapsed parent: %d\n",
        //           cgNode->id(), cgNode->collapsedParent(), cp->id()));
        if (cgNode->collapsedParent() != 0 &&
            cgNode->collapsedParent() != cp->id()) 
        {
          // We found a node which has two different collapsed parent's
          // from different procedures. If both the parents are from the same
          // stInfo, collapse the StInfo, which will indirectly force their
          // collapsed parents to be the same
          ConstraintGraphNode *origcp = 
                    ConstraintGraph::cgNode(cgNode->collapsedParent());
          FmtAssert(origcp->stInfo() == cp->stInfo(),
                    ("Attempting to collapse node: %d that has an existing "
                     "collapsed parent: %d to new collapsed parent: %d\n",
                     cgNode->id(), cgNode->collapsedParent(), cp->id()));
          StInfo *st = origcp->stInfo();
          if (st->checkFlags(CG_ST_FLAGS_MODRANGE)) {
            ModulusRange *outerRange = st->modRange();
            ModulusRange::setModulus(outerRange, 1, _memPool);
          } else
            st->mod(1);
          st->addFlags(CG_ST_FLAGS_ADJUST_MODULUS);
        } else
          cgNode->collapsedParent(cp->id());
      }
    }
  }

  // Update the CGNodeId references in the ConstraintGraphNodes
  for (UINT32 i = 0; i < nodeCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_NODE &summNode = summNodes[nodeIdx + i];
    // Ignore notAPointer CGNodes; they have been already
    // created in the globalConstraintGraph
    if (summNode.cgNodeId() == notAPointer()->id())
      continue;
    ConstraintGraphNode *cgNode = findUniqueNode(summNode.cgNodeId());
    // Set nextOffset
    UINT32 nextOffsetId = summNode.nextOffset();
    if (nextOffsetId != 0) {
      StInfo *stInfo = 
              _ipaCGStIdxToStInfoMap.find(summNode.cg_st_idx())->second;
      FmtAssert(!stInfo->checkFlags(CG_ST_FLAGS_PREG),
                ("PREGs should have no nextOffset"));
      ConstraintGraphNode *nextOffset = findUniqueNode(nextOffsetId);
      bool added = addCGNodeInSortedOrder(stInfo, nextOffset);
      if (added)     
        stInfo->incrNumOffsets();
    }

    // Add the pts set
    UINT32 numBitsPtsGBL = summNode.numBitsPtsGBL();
    UINT32 numBitsPtsHZ  = summNode.numBitsPtsHZ();
    UINT32 numBitsPtsDN  = summNode.numBitsPtsDN();
    UINT32 ptsGBLidx     = summNode.ptsGBLidx();
    UINT32 ptsHZidx      = summNode.ptsHZidx();
    UINT32 ptsDNidx      = summNode.ptsDNidx();

    // GBL
    for (UINT32 i = 0; i < numBitsPtsGBL; i++) {
      CGNodeId id = (CGNodeId)nodeIds[ptsGBLidx + i];
      ConstraintGraphNode *pNode = findUniqueNode(id);
      bool change = false;
      if (pNode->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
        FmtAssert(pNode != pNode->parent(), ("Expecting a distinct parent "
                  "for COLLAPSED node: %d\n", pNode->id()));
        change = cgNode->parent()->addPointsTo(
                 ConstraintGraph::cgNode(pNode->collapsedParent()), CQ_GBL);
      } else
        change = cgNode->parent()->addPointsTo(pNode, CQ_GBL);
      // Mark a changed global as modified so that its outgoing edges
      // gets processed in the solver 
      if (change && cgNode->parent()->cg() == globalCG()) {
        ConstraintGraph::solverModList()->push(cgNode->parent());
      }
    }
    cgNode->parent()->sanitizePointsTo(CQ_GBL);
    Is_True(cgNode->parent()->sanityCheckPointsTo(CQ_GBL),(""));

    // HZ
    for (UINT32 i = 0; i < numBitsPtsHZ; i++) {
      CGNodeId id = (CGNodeId)nodeIds[ptsHZidx + i];
      ConstraintGraphNode *pNode = findUniqueNode(id);
      bool change = false;
      if (pNode->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
        FmtAssert(pNode != pNode->parent(), ("Expecting a distinct parent "
                  "for COLLAPSED node: %d\n", pNode->id()));
        change = cgNode->parent()->addPointsTo(
                 ConstraintGraph::cgNode(pNode->collapsedParent()), CQ_HZ);
      } else
        change = cgNode->parent()->addPointsTo(pNode, CQ_HZ);
      // Mark a changed global as modified so that its outgoing edges
      // gets processed in the solver 
      if (change && cgNode->parent()->cg() == globalCG()) {
        ConstraintGraph::solverModList()->push(cgNode->parent());
      }
    }
    cgNode->parent()->sanitizePointsTo(CQ_HZ);
    Is_True(cgNode->parent()->sanityCheckPointsTo(CQ_HZ),(""));

    // DN
    for (UINT32 i = 0; i < numBitsPtsDN; i++) {
      CGNodeId id = (CGNodeId)nodeIds[ptsDNidx + i];
      ConstraintGraphNode *pNode = findUniqueNode(id);
      bool change = false;
      if (pNode->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
        FmtAssert(pNode != pNode->parent(), ("Expecting a distinct parent "
                  "for COLLAPSED node: %d\n", pNode->id()));
        change = cgNode->parent()->addPointsTo(
                 ConstraintGraph::cgNode(pNode->collapsedParent()), CQ_DN);
      } else
        change = cgNode->parent()->addPointsTo(pNode, CQ_DN);
      if (change && cgNode->parent()->cg() == globalCG()) {
        ConstraintGraph::solverModList()->push(cgNode->parent());
      }
    }
    cgNode->parent()->sanitizePointsTo(CQ_DN);
    Is_True(cgNode->parent()->sanityCheckPointsTo(CQ_DN),(""));

    // Adjust pts set if required
    if (cgNode->checkFlags(CG_NODE_FLAGS_ADJUST_K_CYCLE)) {
      adjustPointsToForKCycle(cgNode);
      cgNode->clearFlags(CG_NODE_FLAGS_ADJUST_K_CYCLE);
    }

    // Handle the case where the node has an existing parent
    UINT32 repParentId = summNode.repParent();
    if (repParentId != 0 && cgNode->repParent() != NULL) {
      ConstraintGraphNode *newRepParent =
                           findUniqueNode(repParentId)->findRep();
      ConstraintGraphNode *oldRepParent = cgNode->findRep();
      if (oldRepParent != newRepParent) {
        // Merge with the new parent
        newRepParent->merge(oldRepParent);
        // Set the newParent as the parent of oldRepParent
        oldRepParent->repParent(newRepParent);
        if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
          fprintf(stderr, "Merging oldRepParent %d with newRepParent %d for "
                  "node: %d (%s)\n", oldRepParent->id(), newRepParent->id(),
                  cgNode->id(), (cgNode->cg() == globalCG()) ? "global" 
                  : "local");
      }
    }
  }

  // A node that is marked COLLAPSED , could still be in the offset
  // list and pts set of another node when processing an earlier procedure
  // So remove from the offset list and pts set
  for (UINT32 i = 0; i < stInfoCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_STINFO &summStInfo = summStInfos[stInfoIdx + i];
    if (summStInfo.firstOffset() == notAPointer()->id())
      continue;
    StInfo *stInfo = 
            _ipaCGStIdxToStInfoMap.find(summStInfo.cg_st_idx())->second;
    if (stInfo->checkFlags(CG_ST_FLAGS_PREG))
      continue;
    ConstraintGraphNode *prev = stInfo->firstOffset();
    ConstraintGraphNode *n = prev->nextOffset();
    while (n) {
      if (n->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
        // We replace n from the pts to set of CGNodes that point to n
        // with n's collapsedParent
        // Use the reverse pts to set to find nodes that point to n and
        // replace by the collapsedParent
        for (PointsToIterator pti(n,PtsRev); pti != 0; ++pti) {
          PointsTo &rpts = *pti;
          CGEdgeQual qual = pti.qual();
          for (PointsTo::SparseBitSetIterator sbsi(&rpts,0);
               sbsi != 0; ++sbsi) {
            ConstraintGraphNode *ptrNode = ConstraintGraph::cgNode(*sbsi);
            ptrNode->removePointsTo(n->id(), qual);
            ptrNode->addPointsTo(ConstraintGraph::cgNode(n->collapsedParent()),
                                 qual);
          }
        }
        n->deleteRevPointsToSet();
        prev->nextOffset(n->nextOffset());
        n->nextOffset(NULL);
        n = prev->nextOffset();
      } else {
        prev = n;
        n = n->nextOffset();
      }
    }
  }

  // Add the ConstraintGraphEdges
  UINT32 edgeIdx = proc->Get_constraint_graph_edges_idx();
  UINT32 edgeCount = proc->Get_constraint_graph_edges_count();
  SUMMARY_CONSTRAINT_GRAPH_EDGE *summEdges = 
          IPA_get_constraint_graph_edges_array(ipaNode->File_Header(), size);
  FmtAssert(edgeCount <= size, ("Invalid edgeCount"));
  for (UINT32 i = 0; i < edgeCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_EDGE &summEdge = summEdges[edgeIdx + i];
    ConstraintGraphNode *srcNode = findUniqueNode(summEdge.src());
    ConstraintGraphNode *destNode = findUniqueNode(summEdge.dest());
    bool added = false;
    ConstraintGraphNode *srcParent = srcNode->parent();
    // If the parent of the destination node is actually the source
    // of the end, we must respect the original parent relationship
    // no additional merging has happened.
    ConstraintGraphNode *destParent;
    if (destNode->parent() == srcParent && 
        (summEdge.flags() & CG_EDGE_PARENT_COPY))
      destParent = destNode;
    else
      destParent = destNode->parent();

    // If src/dest is not a pointer, do not add edges
    if (srcParent->checkFlags(CG_NODE_FLAGS_NOT_POINTER) ||
        destParent->checkFlags(CG_NODE_FLAGS_NOT_POINTER))
      continue;

    // Add the edges to the representative parents
    ConstraintGraphEdge *edge = 
      ConstraintGraph::addEdge(srcParent, destParent,
                               (CGEdgeType)summEdge.etype(),
                               (CGEdgeQual)summEdge.qual(), 
                               summEdge.sizeOrSkew(), added, summEdge.flags());
    // In case the edge already exists, just add the flags
    if (!added)
      edge->addFlags(summEdge.flags());
  }

  // Add the formal paramters and return
  UINT32 parmIdx = proc->Get_constraint_graph_formal_parm_idx();
  UINT32 pcount = proc->Get_constraint_graph_formal_parm_count();
  UINT32 retIdx = proc->Get_constraint_graph_formal_ret_idx();
  UINT32 retCount = proc->Get_constraint_graph_formal_ret_count();
  for (UINT32 i = 0; i < pcount; i++) {
    CGNodeId id = (CGNodeId)nodeIds[parmIdx + i];   
    ConstraintGraphNode *cn = findUniqueNode(id);
    parameters().push_back(cn->id());
  }
  for (UINT32 i = 0; i < retCount; i++) {
    CGNodeId id = (CGNodeId)nodeIds[retIdx + i];   
    ConstraintGraphNode *cn = findUniqueNode(id);
    returns().push_back(cn->id());
  }

  // Add CallSites
  UINT32 callSiteIdx = proc->Get_constraint_graph_callsites_idx();
  UINT32 callSiteCount = proc->Get_constraint_graph_callsites_count();
  SUMMARY_CONSTRAINT_GRAPH_CALLSITE *summCallSites = 
          IPA_get_constraint_graph_callsites_array(ipaNode->File_Header(),
                                                   size);
  FmtAssert(callSiteCount <= size, ("Invalid callSiteCount"));
  for (UINT32 i = 0; i < callSiteCount; i++) {
    SUMMARY_CONSTRAINT_GRAPH_CALLSITE &summCallSite = 
                                       summCallSites[callSiteIdx + i];
    CallSiteId newCSId = nextCallSiteId++;
    CallSite *cs = CXX_NEW(CallSite(newCSId, summCallSite.flags(),
                                    _memPool), _memPool);
    _callSiteMap[newCSId] = cs;
    _uniqueCallSiteIdMap[summCallSite.id()] = cs;
    csIdToCallSiteMap[newCSId] = cs;
    if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
      fprintf(stderr, "Adding CSoldId: %d to CSnewId: %d\n",
              summCallSite.id(), newCSId);
    
    cs->setActualModeled(summCallSite.actualModeled());
    if (cs->checkFlags(CS_FLAGS_VIRTUAL))
      cs->virtualClass(summCallSite.virtualClass());

    if (cs->isDirect() && !cs->isIntrinsic())
      cs->st_idx(summCallSite.st_idx());
    else if (cs->isIndirect())
      cs->cgNodeId(findUniqueNode(summCallSite.cgNodeId())->id());
    else if (cs->isIntrinsic())
      cs->intrinsic(summCallSite.intrinsic());

    UINT32 paramIdx = summCallSite.parmNodeIdx();
    UINT32 parmCount = summCallSite.numParms();
    for (UINT32 i = 0; i < parmCount; i++) {
      CGNodeId id = (CGNodeId)nodeIds[paramIdx + i];   
      ConstraintGraphNode *cn = findUniqueNode(id);
      cs->addParm(cn->id());
    }
    CGNodeId retId = summCallSite.returnId();
    if (retId != 0) {
      ConstraintGraphNode *cn = findUniqueNode(retId);
      cs->returnId(cn->id());
    }
  }
  _buildComplete = true;
}

ConstraintGraphNode *
ConstraintGraph::buildCGNode(SUMMARY_CONSTRAINT_GRAPH_NODE *summ,
                             IPA_NODE *ipaNode)
{
  // Adjust cg_st_idx with the current pu and file index for non-globals
  CG_ST_IDX cg_st_idx = summ->cg_st_idx();
  if (this != globalCG())
    cg_st_idx = adjustCGstIdx(ipaNode, cg_st_idx);

  // The global CG might already have this ConstraintGraphNode
  ConstraintGraphNode *cgNode = checkCGNode(cg_st_idx, summ->offset());
  if (cgNode != NULL) {
    FmtAssert(this == globalCG(), ("Expect this to be the globalCG"));
    // Merge inKCycle
    if (summ->inKCycle() != 0 && summ->inKCycle() != cgNode->inKCycle())
      cgNode->addFlags(CG_NODE_FLAGS_ADJUST_K_CYCLE);
    cgNode->inKCycle(gcd(cgNode->inKCycle(), summ->inKCycle()));

    // Clear the MERGE flag, since if this node has a parent
    // we will be merging this node with the parent and the MERGED flag
    // will be set.
    UINT32 flags = (summ->flags() & (~CG_NODE_FLAGS_MERGED));
    cgNode->addFlags(flags);
    if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
      fprintf(stderr, "Global entry found for CGNode oldId: %d newId: %d "
              " old cg_st_idx: %llu new cg_st_idx: %llu flags: 0x%x\n",
              summ->cgNodeId(), cgNode->id(), summ->cg_st_idx(), cg_st_idx,
              cgNode->flags());
    return cgNode;
  }
 
  // Remap the CGNodeId to a unique id
  CGNodeId oldCGNodeId = summ->cgNodeId(); 
  CGNodeId newCGNodeId = nextCGNodeId++;
  cgNode = CXX_NEW(ConstraintGraphNode(cg_st_idx, summ->offset(), 
                                       summ->flags(), summ->inKCycle(),
                                       newCGNodeId, this), _memPool);
  cgNode->ty_idx(summ->ty_idx());

  // Add to maps in the current ConstraintGraph
  cgIdToNodeMap[newCGNodeId] = cgNode;
  _cgNodeToIdMap[cgNode] = newCGNodeId;

  if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
    fprintf(stderr, "Adding CGNode oldId: %d newId: %d to %s"
            " old cg_st_idx: %llu new cg_st_idx: %llu flags: 0x%x\n",
            summ->cgNodeId(), cgNode->id(),
            (this == globalCG()) ? "global" : "local",
            summ->cg_st_idx(), cg_st_idx, cgNode->flags());

  return cgNode;
}

StInfo *
ConstraintGraph::buildStInfo(SUMMARY_CONSTRAINT_GRAPH_STINFO *summ,
                             IPA_NODE *ipaNode)
{
  // Adjust cg_st_idx with the current pu and file index for non-globals
  CG_ST_IDX cg_st_idx = summ->cg_st_idx();
  if (this != globalCG())
    cg_st_idx = adjustCGstIdx(ipaNode, cg_st_idx);

  // The global CG might already have this StInfo
  CGStInfoMapIterator iter = _cgStInfoMap.find(cg_st_idx);
  if (iter != _cgStInfoMap.end()) {
    StInfo *stInfo = iter->second;
    FmtAssert(this == globalCG(), ("Expect this to be the globalCG"));
    // We expect the variable sizes and types to be consistent, however
    // in the case of forward and extern declarations we may find ourselves
    // having different variable sizes and even different types.
    // In the situation where the variable size is different, we expect
    // one of them to be size zero, that being the forward declaration.
    // We will keep the type, size, modulus of the non-zero sized StInfo.
    if (stInfo->varSize() != summ->varSize()) {
      if (stInfo->varSize() == 0) {
        stInfo->varSize(summ->varSize());
        stInfo->ty_idx(summ->ty_idx());
        stInfo->addFlags(summ->flags());
        // We are checking the flags on the current stInfo because we
        // merged the flags above.
        if (!(stInfo->flags() & CG_ST_FLAGS_MODRANGE))
          stInfo->mod(summ->modulus());
        else
          stInfo->modRange(buildModRange(summ->modulus(),ipaNode));
        return stInfo;
      }
      else {
        // FmtAssert(summ->varSize() == 0,
        //             ("Inconsistent varsize, expect zero\n"));
        if (summ->varSize() < stInfo->varSize()) {
          stInfo->varSize(summ->varSize());
          stInfo->ty_idx(summ->ty_idx());
        }
      }
    }
    
    //FmtAssert(stInfo->ty_idx() == summ->ty_idx(), ("Inconsistent ty_idx"));
    // Merge mod ranges/modulus
    if (!stInfo->checkFlags(CG_ST_FLAGS_MODRANGE)) {
      if (summ->flags() & CG_ST_FLAGS_MODRANGE) {
        ModulusRange *newMR = buildModRange(summ->modulus(), ipaNode);
        FmtAssert(newMR != NULL, ("New ModulusRange not found"));
        stInfo->mod(gcd(stInfo->mod(), newMR->mod()));
        ModulusRange::removeRange(newMR, _memPool);
        stInfo->addFlags(CG_ST_FLAGS_ADJUST_MODULUS);
        if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
          fprintf(stderr, "Adjust modulus for global st_idx: %llu\n", 
                  cg_st_idx);
      } else if (stInfo->mod() != summ->modulus()) {
        stInfo->mod(gcd(stInfo->mod(), summ->modulus()));
        stInfo->addFlags(CG_ST_FLAGS_ADJUST_MODULUS);
        if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
          fprintf(stderr, "Adjust modulus for global st_idx: %llu\n", 
                  cg_st_idx);
      }
    } else {
      // If stInfo has a MODRANGE, and summary does not or if their
      // mod ranges mismatch, then collapse
      ModulusRange *oldMR = stInfo->modRange();
      if (summ->flags() & CG_ST_FLAGS_MODRANGE) {
        ModulusRange *newMR = buildModRange(summ->modulus(), ipaNode);
        FmtAssert(oldMR != NULL, ("Old ModulusRange not found"));
        FmtAssert(newMR != NULL, ("New ModulusRange not found"));
        // If we have a range mismatch, collapse
        if (!newMR->compare(oldMR)) {
          ModulusRange::setModulus(oldMR, 
                                   gcd(oldMR->mod(), newMR->mod()),
                                   stInfo->memPool());
          ModulusRange::removeRange(newMR, _memPool);
          stInfo->addFlags(CG_ST_FLAGS_ADJUST_MODULUS);
          if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
            fprintf(stderr, "Adjust modulus for global st_idx: %llu\n",
                    cg_st_idx);
        }
      } else {
        ModulusRange::setModulus(oldMR, gcd(oldMR->mod(), summ->modulus()),
                                 stInfo->memPool());
        stInfo->addFlags(CG_ST_FLAGS_ADJUST_MODULUS);
        if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
          fprintf(stderr, "Adjust modulus for global st_idx: %llu\n", 
                  cg_st_idx);
      }
    }
    bool noModRange = !stInfo->checkFlags(CG_ST_FLAGS_MODRANGE);
    stInfo->addFlags(summ->flags());
    if (noModRange)
      stInfo->clearFlags(CG_ST_FLAGS_MODRANGE);
    if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
      fprintf(stderr, "Global entry found for StInfo old cg_st_idx: %llu "
              "new cg_st_idx: %llu\n", summ->cg_st_idx(), cg_st_idx);
    return stInfo;
  }
     
  UINT32 flags = summ->flags(); 
  INT64 varSize = summ->varSize(); 
  TY_IDX ty_idx = summ->ty_idx();
  StInfo *stInfo = CXX_NEW(StInfo(flags, varSize, ty_idx, _memPool), _memPool);
  if (flags & CG_ST_FLAGS_MODRANGE) {
    // Read in the ModRanges
    ModulusRange *mr = buildModRange(summ->modulus(), ipaNode);
    stInfo->modRange(mr);
  } else {
    UINT32 modulus = summ->modulus();
    stInfo->mod(modulus);
  }
  // Adjust cg_st_idx with the current pu and file index
  _cgStInfoMap[cg_st_idx] = stInfo;

  // Find max local st_idx 
  SYMTAB_IDX level = ST_IDX_level(CG_ST_IDX(summ->cg_st_idx()));
  if (level == PU_lexical_level(ipaNode->Func_ST())) {
    FmtAssert(this != globalCG(), ("Expect this to be a local CG"));
    UINT32 index     = ST_IDX_index(CG_ST_IDX(summ->cg_st_idx()));
    UINT32 max_index = ST_IDX_index(_max_st_idx);
    FmtAssert(_max_st_idx == 0 || (ST_IDX_level(_max_st_idx) == level),
              ("Inconsistent SYMTAB_IDX"));
    if (index > max_index)
      _max_st_idx = make_ST_IDX(index, level);
  }

  if (Get_Trace(TP_ALIAS,NYSTROM_CG_BUILD_FLAG))
    fprintf(stderr, "Adding StInfo old cg_stIdx: %llu "
            "new cg_st_idx: %llu to %s\n", summ->cg_st_idx(), cg_st_idx,
            (this == globalCG()) ? "global" : "local");
  
  return stInfo;
}

ModulusRange *
ConstraintGraph::buildModRange(UINT32 modRangeIdx, IPA_NODE *ipaNode)
{
  // Get the modrange table
  INT32 size;
  SUMMARY_CONSTRAINT_GRAPH_MODRANGE *summModRanges = 
          IPA_get_constraint_graph_modranges_array(ipaNode->File_Header(),
                                                   size);
  SUMMARY_CONSTRAINT_GRAPH_MODRANGE &summMR = summModRanges[modRangeIdx];
  ModulusRange *mr = CXX_NEW(ModulusRange(summMR.startOffset(), 
                                          summMR.endOffset(),
                                          summMR.modulus(),
					  summMR.ty_idx()), _memPool);
  if (summMR.childIdx() != 0)
    mr->child(buildModRange(summMR.childIdx(), ipaNode));
  if (summMR.nextIdx() != 0)
    mr->next(buildModRange(summMR.nextIdx(), ipaNode));

  return mr;
}



class IPA_EscapeAnalysis : public EscapeAnalysis
{
public:
  IPA_EscapeAnalysis(hash_set<ST_IDX> &extCallSet,
                     IPACGMap &map, bool wholePrgMode,
                     Mode mode, MEM_POOL *memPool) :
    EscapeAnalysis(NULL,false,memPool),
    _extCallSet(extCallSet),
    _ipaCGMap(map)
  {
    ipaMode(mode);
    wholeProgramMode(wholePrgMode);
  }

  void init();
  bool formalsEscape(ConstraintGraph *graph) const;
  bool externalCall(ST_IDX idx) const;

private:
  bool _possibleIndirectCallTarget(ConstraintGraph *graph) const;
  bool _hasDirectPredecessors(ConstraintGraph *graph) const;

  IPACGMap         &_ipaCGMap;
  hash_set<ST_IDX> &_extCallSet;
};

void
IPA_EscapeAnalysis::init(void)
{
  // Walk each of the PU constraint graphs
  IPACGMap &map = _ipaCGMap;
  for (IPACGMapIterator iter = map.begin();
       iter != map.end(); ++iter) {
    initGraph(iter->second);
  }
  // Don't forget about the global constraint graph
  initGraph(ConstraintGraph::globalCG());
}

bool
IPA_EscapeAnalysis::_possibleIndirectCallTarget(ConstraintGraph *graph) const
{
  ST *funcSt = graph->ipaNode()->Func_ST();
  ConstraintGraphNode *clrCGNode =
      ConstraintGraph::globalCG()->checkCGNode(funcSt->st_idx,0);
  if (!clrCGNode || !clrCGNode->checkFlags(CG_NODE_FLAGS_ADDR_TAKEN))
    return false;
  else
    return true;
}

bool
IPA_EscapeAnalysis::_hasDirectPredecessors(ConstraintGraph *graph) const
{
  IPA_NODE *ipaNode = graph->ipaNode();
  IPA_PRED_ITER predIter(ipaNode);
  for (predIter.First(); !predIter.Is_Empty(); predIter.Next())
  {
    IPA_EDGE * edge = predIter.Current_Edge();
    // Have we found a direct call?
    if (edge &&
        !edge->Summary_Callsite()->Is_func_ptr() &&
        !edge->Summary_Callsite()->Is_virtual_call())
      return true;
  }
  return false;
}

bool
IPA_EscapeAnalysis::formalsEscape(ConstraintGraph *graph) const
{
  // Formals are considered to be escaped iff
  // (1) We are not in -ipa mode
  // (2) We are in -ipa mode and the routine is an indirect
  //     call target and we have not resolved all indirect
  //     call edges.
  if (ipaMode() == IPANo)
    return true;

  if (ipaMode() == IPAComplete) {
    // If we have a callee that is not an indirect target yet has no direct
    // predecessors, we must treat its formals as escaping.  A key assumption
    // here is that we have a constraint graph for all possible direct
    // predecessors.
    if (graph != ConstraintGraph::globalCG() &&
        (_possibleIndirectCallTarget(graph) ||  // complete graph
            _hasDirectPredecessors(graph)))     // any callers?
      return false;
  }

  if (ipaMode() == IPAIncomplete) {
    if (graph != ConstraintGraph::globalCG() &&
        !_possibleIndirectCallTarget(graph) &&
        _hasDirectPredecessors(graph))
      return false;
  }

  // Be defensive...
  return true;
}

bool
IPA_EscapeAnalysis::externalCall(ST_IDX idx) const
{
  hash_set<ST_IDX>::iterator iter = _extCallSet.find(idx);
  return (iter != _extCallSet.end());
}

/*
 * Assemble the list of call edges in the IPA call graph, the list of all
 * possible indirect call targets, and all virtual call sites.
 *
 * Eventually we would like to use IPA_EDGEs directly, but we are currently
 * unable to update the call graph as indirect call targets are resolved to
 * multiple targets.
 */
typedef struct
{
  size_t operator() (const ST *k) const { return (size_t)k; }
} hashST;
typedef struct
{
   bool operator() (const ST *k1, const ST *k2) const { return k1 == k2; }
} equalST;

void
IPA_NystromAliasAnalyzer::callGraphSetup(IPA_CALL_GRAPH *ipaCallGraph,
                            list<IPAEdge> &edgeList,
                            list<pair<IPA_NODE *, CallSiteId> > &indirectCallList)
{
  hash_set<ST *,hashST,equalST> calledFunc;
  hash_map<NODE_INDEX,UINT32>   callerCount;

  IPA_NODE_ITER nodeIter(ipaCallGraph,DONTCARE);
  // First a quick walk of the call graph to determine all functions
  // that are actually present within our IPA scope.  The mapping from
  // ST to IPA_NODE will come in handy later.
  for (nodeIter.First(); !nodeIter.Is_Empty(); nodeIter.Next()) {
     IPA_NODE *caller = nodeIter.Current();
     if (caller == NULL) continue;
     ST *st = caller->Func_ST();
     if (cg(caller->Node_Index()) != NULL)
       _stToIPANodeMap[st] = caller;
     // Collect list of possible indirect call targets, here we check
     // to see whether the StInfo associated with this routine is address
     // taken.  Likely if there exists a CGNode for this routine it is
     // address taken, otherwise it would not exist.  Earlier attempts
     // to make use of the ST_addr_passed/ST_addr_save flags for the PU
     // ST provided to be extremely conservative for some reason.
     if (ST_IDX_level(st->st_idx) == GLOBAL_SYMTAB) {
       ConstraintGraphNode *clrCGNode =
           ConstraintGraph::globalCG()->checkCGNode(st->st_idx,0);
       if (clrCGNode && clrCGNode->checkFlags(CG_NODE_FLAGS_ADDR_TAKEN))
         _stToIndTgtMap[st] = caller;
     }
  }

  // Now the real walk to locate the direct/indirect callsites and
  // populate the list of call edges to be connected in the constraint
  // graph.
  for (nodeIter.First(); !nodeIter.Is_Empty(); nodeIter.Next()) {
    IPA_NODE *caller = nodeIter.Current();
    if (caller == NULL) continue;
    ConstraintGraph *graph = cg(caller->Node_Index());
    FmtAssert(graph != NULL, ("ConstraintGraph missing for caller: %s\n",
              caller->Name()));
    if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
      fprintf(stderr,"Processing: %s\n",caller->Name());

    // We use the call site information on the constraint graph, rather
    // than the summary callsite information because transformations
    // may make the summary callsite to CallSite mapping inconsistent.
    for (CallSiteIterator csi = graph->callSiteMap().begin();
        csi != graph->callSiteMap().end(); ++csi) {
      CallSite *callsite = csi->second;
      if (callsite->isDirect() && !callsite->isIntrinsic()) {
        ST_IDX calleeStIdx = callsite->st_idx();
        ST *funcST = &St_Table[calleeStIdx];
        STToNodeMap::iterator iter = _stToIPANodeMap.find(funcST);
        if (iter != _stToIPANodeMap.end()) {
          if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
            fprintf(stderr," Direct call: %s\n",ST_name(funcST));
          IPA_NODE *callee = iter->second;
          edgeList.push_back(IPAEdge(caller->Node_Index(),
                                     callee->Node_Index(),callsite->id()));
          UINT32 &cnt = callerCount[callee->Node_Index()];
          cnt += 1;
        }
        // We are also tracking all called functions for determining
        // external calls
        calledFunc.insert(funcST);
      }
      else if (callsite->isIndirect())
        indirectCallList.push_back(make_pair(caller,callsite->id()));
    }
  }

  // Determine the external calls that are made from this IPA scope
  for (hash_set<ST*,hashST,equalST>::iterator iter = calledFunc.begin();
      iter != calledFunc.end(); ++iter) {
    ST *calledFuncST = *iter;
    STToNodeMap::iterator i3 = _stToIPANodeMap.find(calledFuncST);
    if (i3 == _stToIPANodeMap.end()) {
      bool inTable = false;
      CallSideEffectInfo::GetCallSideEffectInfo(calledFuncST,&inTable);
      if (!inTable && !ST_is_not_used(&St_Table[calledFuncST->st_idx])) {
        _extCallSet.insert(calledFuncST->st_idx);
        if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
          fprintf(stderr,"External call: %s (%d)\n", ST_name(calledFuncST),
                  ST_sclass(calledFuncST));
      }
    }
  }

#if 0
  const UINT32 callerLimit = 200;
  for (hash_map<NODE_INDEX,UINT32>::iterator ix = callerCount.begin();
      ix != callerCount.end(); ++ix) {
    NODE_INDEX nodeIdx = ix->first;
    UINT32 cnt = ix->second;
    if ( cnt > callerLimit && nodeIdx != 1522 && nodeIdx != 2650) {
      ConstraintGraph *calleeCG = cg(nodeIdx);
      calleeCG->doNotConnect(true);
      fprintf(stderr,"Callee %d has %d (> %d) callers: %s\n",
              nodeIdx,cnt,callerLimit,
              ipaCallGraph->Graph()->Node_User(nodeIdx)->Name());
    }
  }
#endif

  if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG)) {

    for (STToNodeMap::iterator i1 = _stToIndTgtMap.begin();
        i1 != _stToIndTgtMap.end(); ++i1) {
      ST *st = i1->first;
      IPA_NODE *node = i1->second;
      fprintf(stderr,"Indirect call target: %s (%d)\n",ST_name(st),node->Node_Index());
    }

    for (list<pair<IPA_NODE *,CallSiteId> >::iterator i2 = indirectCallList.begin();
        i2 != indirectCallList.end(); ++i2) {
      IPA_NODE *caller = i2->first;
      CallSiteId id = i2->second;
      fprintf(stderr,"Indirect call site: IPA_NODE %d, CallSiteId %d\n",
              caller->Node_Index(),id);
    }
  }
}
void
IPA_NystromAliasAnalyzer::callGraphPrep(IPA_CALL_GRAPH *ipaCallGraph,
                                        list<IPAEdge> &workList,
                                        EdgeDelta &delta,
                                        list<IPA_NODE *> &revTopOrder,
                                        UINT32 round)
{
  if(Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG)) {
    fprintf(stderr,"#### start call graph prep ####\n");
  }
  // TODO:  We need to perform SCC detection here and then
  // perform unification after the edges have been added.
  // The topological ordering may be useful in computing the
  // edge delta for the initial round.

  // Connect param/return edges for edges within the workList.
  // The 'delta' is populated with new constraint graph edges
  // to be processed during the next solution pass.  The first
  // time we do this, we will add *all* edges to the delta.
  UINT32 actualSize[32];
  UINT32 paramCnt = 0;
  while (!workList.empty()) {
    IPAEdge edge = workList.front();
    workList.pop_front();

    UINT32 id = edge.csId();

    IPA_NODE *callee = ipaCallGraph->Graph()->Node_User(edge.calleeIdx());
    if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
      fprintf(stderr,"Prep: connect %s (%d) -> %s\n",
              ipaCallGraph->Graph()->Node_User(edge.callerIdx())->Name(),
              id,
              callee->Name());

    // Retrieve the caller/callee constraint graphs
    ConstraintGraph *callerCG = cg(edge.callerIdx());
    ConstraintGraph *calleeCG = cg(edge.calleeIdx());

    if (!callerCG) {
      fprintf(stderr, "callGraphPrep: No CG for caller: %s\n", 
              ipaCallGraph->Graph()->Node_User(edge.callerIdx())->Name());
      continue;
    }
    if (!calleeCG) {
      fprintf(stderr, "callGraphPrep: No CG for callee: %s\n", callee->Name());
      continue;
    }
    if (calleeCG->doNotConnect()) {
      fprintf(stderr, "callGraphPrep: Skipping callee: %s\n", callee->Name());
      continue;
    }
      
    // Now, the real work.  Connect the actuals/formals for this callsite
    callerCG->connect(id,calleeCG,callee->Func_ST(),delta);
  }

  // Perform cycle detection on the call graph and
  // merge nodes within SCCS.  A side-effect of the SCC
  // analysis is the topological order of the acyclic call graph
  if (!workList.empty()) {
    IPASCCDetection scc(this,ipaCallGraph,&_memPool);
    scc.findAndUnify();
    IPASCCDetection::IPANodeStack &topoStack = scc.topoNodeStack();

    // Compute the reverse topological order of the call graph
    if (revTopOrder.empty())
      while (!topoStack.empty()) {
        revTopOrder.push_front(topoStack.top());
        topoStack.pop();
      }
  }
  if(Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG)) {
    fprintf(stderr,"#### end call graph prep ####\n");
  }
}

bool
IPA_NystromAliasAnalyzer::validTargetOfVirtualCall(CallSite *cs, ST_IDX stIdx)
{
  ST *st = &St_Table[stIdx];
  const char *calleeName = ST_name(stIdx);
  if (calleeName[0] == '_' && calleeName[1] == 'Z') {
    TY_IDX func_ty_idx = ST_pu_type(st);
    TYLIST_IDX tylist_index = TY_tylist(Ty_Table[func_ty_idx]);
    // Get the 'this' ptr type, after the return type
    TY_IDX this_ptr_ty_idx = Tylist_Table[++tylist_index];
    if (TY_kind(this_ptr_ty_idx) != KIND_POINTER)
      return false;
    TY_IDX this_ty_idx = TY_pointed(this_ptr_ty_idx);

    // Get callee class
    TY_INDEX calleeVirtClass = TY_IDX_index(this_ty_idx);
    //fprintf(stderr, "calleeVirtclass: %s\n",
    //        TY_name(make_TY_IDX(calleeVirtClass))); 
    // Get callsite class
    TY_INDEX virtClass = cs->virtualClass() >> 8;
    //fprintf(stderr, "virtClass: %s\n", TY_name(make_TY_IDX(virtClass))); 

    if (calleeVirtClass == virtClass ||
        IPA_Class_Hierarchy->Is_Ancestor(virtClass, calleeVirtClass) ||
        IPA_Class_Hierarchy->Is_Ancestor(calleeVirtClass, virtClass))
    {
      //fprintf(stderr, "match\n");
      return true;
    }

    return false;
  }
  // Not a mangled C++ name, cannot be called from virtual callsite
  else {
    //if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
    //  fprintf(stderr, "  virtual call cannot call: %s\n",ST_name(st));
    return false;
  }
}

bool
IPA_NystromAliasAnalyzer::updateCallGraph(IPA_CALL_GRAPH *ipaCallGraph,
                      list<pair<IPA_NODE *,CallSiteId> > &indCallList,
                      list<IPAEdge> &edgeList)
{
  if(Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG)) {
    fprintf(stderr,"#### start update call graph ####\n");
  }
  INT numNewTargets = 0;
  // Walk each indirect call site and determine if there exists
  // an edge in the call graph for each CGNode in the points-to
  // set of the indirect call.
  list<pair<IPA_NODE *,CallSiteId> >::const_iterator iter;
  for (iter = indCallList.begin(); iter != indCallList.end(); ++iter) {
    IPA_NODE *caller = iter->first;
    UINT32 id = iter->second;
    ConstraintGraph *callerCG = cg(caller->Node_Index());
    CallSite *cs = callerCG->callSite(id);
    FmtAssert(cs->isIndirect(),
              ("Expected indirect CallSite in caller constraint graph"));
    if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG)) {
      fprintf(stderr,"Indirect Call: %s (%d) maps to CGNodeId: %d\n",
              caller->Name(),id,cs->cgNodeId());
      fprintf(stderr,"Points To:\n");
    }
    ConstraintGraphNode *csNode = ConstraintGraph::cgNode(cs->cgNodeId());
    for (PointsToIterator pti(csNode); pti != 0; ++pti) {
      PointsTo &curPts = *pti;
      for (PointsTo::SparseBitSetIterator iter(&curPts,0); iter != 0; iter++) {
        CGNodeId nodeId = *iter;
        // If the node is collapsed, inspect other Sts that were collapsed
        // with the node
        ConstraintGraphNode *node = ConstraintGraph::cgNode(nodeId);
        for (; nodeId != 0; nodeId = node->nextCollapsedSt()) {
          node = ConstraintGraph::cgNode(nodeId);
          StInfo *stInfo = node->stInfo();
          if (!stInfo->checkFlags(CG_ST_FLAGS_FUNC)) {
            //if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
            //  fprintf(stderr,"Update Call Graph: callsite points to "
            //          "non-func: cgnode %d\n", node->id());
            continue;
          }
          ST_IDX stIdx = SYM_ST_IDX(node->cg_st_idx());
          ST *st = &St_Table[stIdx];
          STToNodeMap::const_iterator ni = _stToIndTgtMap.find(st);
          if (ni == _stToIndTgtMap.end()) {
            if (ST_sclass(st) != SCLASS_EXTERN) {
              if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
                fprintf(stderr,"Update Call Graph: callsite points to possibly "
                        "deleted func: %s\n", ST_name(st));
            } else {
              if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
                fprintf(stderr,"Update Call Graph: callsite points to extern "
                        "func: %s\n", ST_name(st));
            }
            // assume whole program mode for the moment
            continue;
          }
          // We will skip main (that would be a nice cycle wouldn't it?)
          if (PU_is_mainpu(Pu_Table[ST_pu(st)]))
            continue;

          IPA_NODE *callee = ni->second;
          // If we have a parameter mismatch, then we are likely calling the
          // wrong function.  We are not yet clear how strong our assertion
          // can be at this point, depends on language, etc.  So, for now
          // we experiment with pruning just based on a different number of
          // formals vs. actuals.
          if (cs->parms().size() != callee->Num_Formals()) {
            if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
              fprintf(stderr,"  CGNode: %d, ST: %s, IPA_NODE: %d (skipping)\n",
                                nodeId,ST_name(st),callee->Node_Index());
            continue;
          }

          IPAEdge newEdge(caller->Node_Index(),callee->Node_Index(),id);
          // Have we seen this edge before?
          IndirectEdgeSet::iterator iter = _indirectEdgeSet.find(newEdge);
          bool isNew = (iter == _indirectEdgeSet.end());

          // Okay, we have a virtual call.  Is the current callee a valid
          // potential target of this virtual call?
          if (isNew && cs->checkFlags(CS_FLAGS_VIRTUAL)) {
            if (!validTargetOfVirtualCall(cs,stIdx)) {
              if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
                fprintf(stderr,"  CGNode: %d, ST: %s, IPA_NODE: %d "
                        "(not valid target)\n",
                        nodeId,ST_name(st),callee->Node_Index());
              continue;
            }
          }

          if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
            fprintf(stderr,"  CGNode: %d, ST: %s, IPA_NODE: %d (%s)\n",
                    nodeId,ST_name(st),callee->Node_Index(),
                    isNew?"New":"Old");

          // New edges are placed in our set of resolved indirect target
          // edges and placed in the work list for the next round of
          // call graph preparation.  Eventually we would like to actually
          // add a true IPA_EDGE to the IPA_CALL_GRAPH here.
          if (isNew) {
            _indirectEdgeSet.insert(newEdge);
            edgeList.push_front(newEdge);
            numNewTargets++;
            if (numNewTargets > 50000) {
              fprintf(stderr, "updateCallGraph: too many calls.."
                      "IPA assumed to be incomplete\n");
              return false;
            }
          }
        }
      }
    }
  }
  if(Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG)) {
    fprintf(stderr,"#### end update call graph ####\n");
  }

  return true;
}

bool
IPA_NystromAliasAnalyzer::findIncompleteIndirectCalls(
                              IPA_CALL_GRAPH *ipaCallGraph,
                              list<pair<IPA_NODE *,CallSiteId> > &indCallList,
                              list<IPAEdge> &edgeList,
                              IPA_EscapeAnalysis &escAnal)
{
  // Walk each indirect call side and determine if there exists a
  // "blackhole" in its points-to set.  If that is the case, then
  // the target is determined by an incomplete (escaping) symbol.
  list<pair<IPA_NODE *,CallSiteId> >::const_iterator iter;
  INT numNewTargets = 0;
  for (iter = indCallList.begin(); iter != indCallList.end(); ++iter) {
    IPA_NODE *caller = iter->first;
    UINT32 id = iter->second;
    ConstraintGraph *callerCG = cg(caller->Node_Index());
    CallSite *cs = callerCG->callSite(id);
    FmtAssert(cs->isIndirect(),
              ("Expected indirect CallSite in caller constraint graph"));
    ConstraintGraphNode *icallNode = ConstraintGraph::cgNode(cs->cgNodeId());
    if (escAnal.escaped(icallNode)) {
      // We have an incomplete indirect call, now connect it up to "everything"
      if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
        fprintf(stderr,
                "Incomplete Indirect Call: %s (%d) %s maps to CGNodeId: %d\n",
                caller->Name(),id,icallNode->stName(),cs->cgNodeId());
      STToNodeMap::const_iterator iter = _stToIndTgtMap.begin();
      for (; iter != _stToIndTgtMap.end(); ++iter) {
        const ST *st = iter->first;
        IPA_NODE *ipaNode = iter->second;
        // If we have a parameter mismatch, then we are likely calling the
        // wrong function.  We are not yet clear how strong our assertion
        // can be at this point, depends on language, etc.  So, for now
        // we experiment with pruning just based on a different number of
        // formals vs. actuals.
        if (cs->parms().size() != ipaNode->Num_Formals())
          continue;

        // Have we seen this edge before?
        IPAEdge newEdge(caller->Node_Index(),ipaNode->Node_Index(),id);
        IndirectEdgeSet::iterator iter = _indirectEdgeSet.find(newEdge);
        bool isNew = (iter == _indirectEdgeSet.end());

        // Okay, we have a virtual call.  Is the current callee a valid
        // potential target of this virtual call?
        if (isNew && cs->checkFlags(CS_FLAGS_VIRTUAL)) {
          ST_IDX stIdx = st->st_idx;
          //ST_IDX stIdx = SYM_ST_IDX(ipaNode->cg_st_idx());
          if (!validTargetOfVirtualCall(cs,stIdx)) {
            if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
              fprintf(stderr,"  ST: %s, IPA_NODE: %d (not valid target)\n",
                      ST_name(st),ipaNode->Node_Index());
            continue;
          }
        }

        // New edges are placed in our set of resolved indirect target
        // edges and placed in the work list for the next round of
        // call graph preparation.  Eventually we would like to actually
        // add a true IPA_EDGE to the IPA_CALL_GRAPH here.
        if (isNew) {
          _indirectEdgeSet.insert(newEdge);
          edgeList.push_front(newEdge);
          if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
            fprintf(stderr,"  Now calls %s\n",ST_name(st));
          numNewTargets++;
          if (numNewTargets > 75000) {
            fprintf(stderr, "findIncompleteIndirectCalls: too many calls.."
                    "IPA assumed to be incomplete\n");
            return false;
          }
        }
      }
    }
  }
  return true;
}

void
IPA_NystromAliasAnalyzer::solver(IPA_CALL_GRAPH *ipaCallGraph)
{
  UINT32 round = 0;
  bool contextSensitive = false;
  void *sbrk1 = sbrk(0);

  // Call applyModulus on the global StInfos to fixup any offsets if the modulus
  // has changed during StInfo merging during buildCGipa
  for (CGStInfoMapIterator iter = 
       ConstraintGraph::globalCG()-> stInfoMap().begin();
       iter != ConstraintGraph::globalCG()->stInfoMap().end(); iter++) {
    StInfo *stInfo = iter->second;
    if (stInfo->checkFlags(CG_ST_FLAGS_ADJUST_MODULUS)) {
      stInfo->applyModulus();
      stInfo->clearFlags(CG_ST_FLAGS_ADJUST_MODULUS);
    }
  }

  //ConstraintGraph::ipaSimpleOptimizer();

  if (Get_Trace(TP_ALIAS, NYSTROM_SOLVER_FLAG) ||
      Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG))
    fprintf(stderr,"IPA Nystrom: Solver Begin\n");

  if (Get_Trace(TP_ALIAS, NYSTROM_MEMORY_TRACE_FLAG)) {
    fprintf(TFile,"Memory usage before IPA solve.\n");
    MEM_Trace();
    ConstraintGraph::stats();
  }

  // Here we have a single driver that supports both a
  // context-sensitive and context-insensitive solutions.
  //
  // Context-sensitive:  The context sensitive solution uses
  // a iterative top-down, bottom-up approach that continues
  // until we can make no future refinements to the call graph
  // The bottom-up pass only is context sensitive
  //
  // Context-insensitive:  We perform iterative solution of the
  // entire global constraint graph.

  // Assemble a list of all call edges in the call graph to
  // serve as the seed input to the solution process.  On
  // subsequent iterations the work list will comprise only
  // those edges that correspond to newly discovered indirect
  // call targets.
  list<IPAEdge> edgeList;
  list<pair<IPA_NODE *,CallSiteId> > indirectCallList;
  callGraphSetup(ipaCallGraph,edgeList,indirectCallList);

  bool change;
  list<IPA_NODE *> revTopOrder;
  EdgeDelta delta;
  // Provide the constraint graph with a handle on the worklist
  // to ensure proper update during edge deletion
  ConstraintGraph::workList(&delta);
  bool completeIndirect = true;
  do {
    change = false;
    round++;
    FmtAssert(revTopOrder.empty(),("solver: expected rev top order list empty"));

    if (Get_Trace(TP_ALIAS, NYSTROM_SOLVER_FLAG) ||
        Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG)) {
      fprintf(stderr,"#### Round %d ####\n",round);
      fprintf(stderr,"#### Round %d Top-Down ####\n",round);
    }

    // Top down
    do {
      // Perform SCC detection and collapse within the call graph,
      // connect param/return nodes.  This step will provide
      // 1) A topological ordering of call graph for bottom-up
      // 2) The edge delta for the solution.  Note, the edge delta
      //    for the solution consists only of those new copy edges
      //    added to the graph.  Even for the first iteration, we
      //    assume that we are starting from a valid local solution
      //    computed during IPL.
      callGraphPrep(ipaCallGraph,edgeList,delta,revTopOrder,round);
      FmtAssert(edgeList.empty(),
                ("solver: Expected all call edges to be processed"));

      if (delta.empty())
        break;

      // Now, we solve the graph
      ConstraintGraphSolve cgsolver(delta,NULL,&_memPool);
      if (!cgsolver.solveConstraints()) {
        if (Get_Trace(TP_ALIAS, NYSTROM_SOLVER_FLAG)) {
          ConstraintGraphVCG::dumpVCG("ipa_failed_soln");
          fprintf(stderr,"IPA Nystrom: No solution found, likely unknown write\n");
        }
        ConstraintGraph::workList(NULL);
        return;
      }

      // Determine if there have been any changes to call sites
      // and update the call graph
      completeIndirect = 
        updateCallGraph(ipaCallGraph,indirectCallList,edgeList);

      if (!completeIndirect)
        break;

    } while (1);

    if (Get_Trace(TP_ALIAS, NYSTROM_SOLVER_FLAG) ||
        Get_Trace(TP_ALIAS, NYSTROM_LW_SOLVER_FLAG))
       fprintf(stderr,"#### Round %d Bottom-Up ####\n",round);

    // Bottom up
    if (contextSensitive) {
      // Walk the call graph in reverse topological order, using
      // the topological ordering found during SCC detection
      while (!revTopOrder.empty()) {
        IPA_NODE *curNode = revTopOrder.front();
        revTopOrder.pop_front();

        // Apply summaries for the callees of the current routine.
        // This provides the edge delta for the solution.
        ConstraintGraph *curCG = cg(curNode->Node_Index());
        FmtAssert(delta.empty(),("Bottom-Up: expect delta empty after solve"));
        curCG->applyCalleeSummaries(delta);

        // Now, we solve the graph.  We actually perform cycle
        // detect within the the current constraint graph
        ConstraintGraphSolve cgsolver(delta,curCG,&_memPool);
        if (!cgsolver.solveConstraints()) {
          if (Get_Trace(TP_ALIAS, NYSTROM_SOLVER_FLAG))
            fprintf(stderr,"IPA Nystrom: No solution found, likely unknown write\n");
          ConstraintGraph::workList(NULL);
          return;
        }
      }
    }

    // Determine if there have been any changes to call sites
    // and update the call graph
    FmtAssert(delta.empty(),("Expect delta empty after solve"));
    if (completeIndirect)
      completeIndirect = 
        updateCallGraph(ipaCallGraph,indirectCallList,edgeList);

    if (!completeIndirect)
      break;

  } while (change);

  if (completeIndirect)
  { 
    // We perform escape analysis to determine which symbols may point
    // to memory that is not visible within our scope.
    IPA_EscapeAnalysis escAnal(_extCallSet,
                               _ipaConstraintGraphs,
                               IPA_Enable_Whole_Program_Mode,
                               EscapeAnalysis::IPAIncomplete,
                               &_memPool);
    escAnal.perform();

    if (Get_Trace(TP_ALIAS,NYSTROM_MEMORY_TRACE_FLAG)) {
      void *sbrk2 = sbrk(0);
      fprintf(stderr,"High water: %d after core solver\n",
              (char *)sbrk2 - (char *)sbrk1);
      fprintf(TFile,("Memory usage after core solver, before fixup.\n"));
      MEM_Trace();
    }

    // If at this point we still have incomplete indirect calls, then
    // we must attach them to possible callee's and perform a final
    // round of solution.  Initially, this may attempt to connect an
    // indirect call with all possible callees.
    completeIndirect = 
      findIncompleteIndirectCalls(ipaCallGraph,indirectCallList,
                                  edgeList,escAnal);
  }
  if (completeIndirect && !edgeList.empty())  {
    callGraphPrep(ipaCallGraph,edgeList,delta,revTopOrder,round);
    if (!delta.empty()) {
      ConstraintGraphSolve cgsolver(delta,NULL,&_memPool);
      cgsolver.solveConstraints();
    }
  }

  // Print solver statistics for all iterations
  if (Get_Trace(TP_ALIAS,NYSTROM_LW_SOLVER_FLAG))
    ConstraintGraphSolve::printStats();

  if (Get_Trace(TP_ALIAS,NYSTROM_MEMORY_TRACE_FLAG)) {
    void *sbrk2 = sbrk(0);
    fprintf(stderr,"High water: %d after indirect update solve\n",
                (char *)sbrk2 - (char *)sbrk1);
  }

  IPA_EscapeAnalysis escAnalFinal(_extCallSet,
                                  _ipaConstraintGraphs,
                                  IPA_Enable_Whole_Program_Mode,
                                  completeIndirect
                                    ? EscapeAnalysis::IPAComplete
                                    : EscapeAnalysis::IPAIncomplete,
                                  &_memPool);
  escAnalFinal.perform();
  escAnalFinal.markEscaped();

  if (Get_Trace(TP_ALIAS,NYSTROM_MEMORY_TRACE_FLAG)) {
    void *sbrk2 = sbrk(0);
    fprintf(stderr,"High water: %d after final escape analysis\n",
                (char *)sbrk2 - (char *)sbrk1);
    fprintf(TFile,("Memory usage after IPA solve.\n"));
    MEM_Trace();
    ConstraintGraph::stats();
  }

  if (Get_Trace(TP_ALIAS,NYSTROM_CG_POST_FLAG)) {
    fprintf(stderr, "Printing final ConstraintGraphs...\n");
    IPA_NystromAliasAnalyzer::aliasAnalyzer()->print(stderr);
  }

  // post-process the points-to sets
  ConstraintGraphSolve::postProcessPointsTo();

  ConstraintGraph::workList(NULL);

  if (Get_Trace(TP_ALIAS, NYSTROM_SOLVER_FLAG))
     fprintf(stderr,"IPA Nystrom: Solver Complete\n");

  _indirectEdgeSet.clear();
  _stToIndTgtMap.clear();
  _stToIPANodeMap.clear();
  _extCallSet.clear();

  if (Get_Trace(TP_ALIAS,NYSTROM_CG_VCG_FLAG))
    ConstraintGraphVCG::dumpVCG("ipa_final");
}

CG_ST_IDX
ConstraintGraph::buildLocalStInfo(TY_IDX ty_idx)
{
  StInfo *stInfo = CXX_NEW(StInfo(ty_idx, CG_ST_FLAGS_IPA_LOCAL, _memPool),
                           _memPool);
  UINT32 index     = ST_IDX_index(_max_st_idx);
  SYMTAB_IDX level = ST_IDX_level(_max_st_idx);
  index++;
  _max_st_idx = make_ST_IDX(index, level);
  // Record the newly generated local StInfos
  CG_ST_IDX cg_st_idx = adjustCGstIdx(_ipaNode, _max_st_idx);
  _newLocalStInfos[cg_st_idx] = stInfo;
  _cgStInfoMap[cg_st_idx] = stInfo;
  return cg_st_idx;
}

// Check if a preg node has a single outgoing copy edge and return its type
static bool 
copyTarget(ConstraintGraphNode *node, TY_IDX &ty_idx)
{
  if (!node->stInfo()->checkFlags(CG_ST_FLAGS_PREG))
    return false;
  // Check if node has a single outgoing copy target
  if (!node->outLoadStoreEdges().empty())
    return false;
  if (!node->inLoadStoreEdges().empty())
    return false;
  if (!node->inCopySkewEdges().empty())
    return false;
  const CGEdgeSet &edgeSet = node->outCopySkewEdges();
  if (edgeSet.size() != 1)
    return false;
  CGEdgeSetIterator eiter = edgeSet.begin();
  ConstraintGraphEdge *edge = *eiter;
  if (edge->edgeType() != ETYPE_COPY)
    return false;
  ty_idx = edge->destNode()->parent()->stInfo()->ty_idx();
  return true;
}

// Here we are connecting the actuals from the provided callsite
// to the formals of the callee constraint graph.  Any new edges
// added are inserted into the edge "delta" for the next solution
// pass.
void
ConstraintGraph::connect(CallSiteId id, ConstraintGraph *callee,
                         ST *calleeST, EdgeDelta &delta)
{
  CallSite *cs = callSite(id);

  // Connect actual parameters in caller to formals of callee
  list<CGNodeId>::const_iterator actualIter = cs->parms().begin();
  list<CGNodeId>::const_iterator formalIter = callee->parameters().begin();
  ConstraintGraphNode *lastFormal = NULL;
  for (; actualIter != cs->parms().end() && 
       formalIter != callee->parameters().end(); ++actualIter, ++formalIter) {
    ConstraintGraphNode *actual = cgNode(*actualIter);
    ConstraintGraphNode *formal = cgNode(*formalIter);
    lastFormal = formal;
    if (actual->checkFlags(CG_NODE_FLAGS_NOT_POINTER) ||
        formal->checkFlags(CG_NODE_FLAGS_NOT_POINTER))
      continue;

    bool added = false;
    INT64 size = formal->stInfo()->varSize();
    list<ConstraintGraphEdge *> edgeList;
    // if formal is fortran's formal_ref parameter, it's passed by reference
    // caller pass a pointer to callee, callee access formal parameter by value.
    // this will be lowered until cg lowering.
    // so add load edge from actual to formal_ref.
    if(formal->checkFlags(CG_NODE_FLAGS_FORMAL_REF_PARAM))  {
      added = addPtrAlignedEdges(actual->parent(),formal->parent(),
                               ETYPE_LOAD, CQ_DN,size,edgeList);
    }
    else {
      added = addPtrAlignedEdges(actual->parent(),formal->parent(),
                               ETYPE_COPY,CQ_DN,size,edgeList);
    }
    if (added)
      delta.add(edgeList);
  }

  // If we have more actuals than formals either we either have a
  // signature mismatch or varargs.  For now we don't worry about
  // the other mismatch cases.
  if (actualIter != cs->parms().end() &&
      formalIter == callee->parameters().end() &&
      TY_is_varargs(ST_pu_type(calleeST))) {
    // Hook up remaining actuals to the "varargs" node
    FmtAssert(callee->stInfo(lastFormal->cg_st_idx())
                  ->checkFlags(CG_ST_FLAGS_VARARGS),
                  ("Expect last formal to be varargs!\n"));
    for ( ; actualIter != cs->parms().end(); ++actualIter) {
      ConstraintGraphNode *actual = cgNode(*actualIter);
      if (actual->checkFlags(CG_NODE_FLAGS_NOT_POINTER))
        continue;
      INT64 size = actual->stInfo()->varSize();
      bool added = false;
      list<ConstraintGraphEdge *> edgeList;
      added = addPtrAlignedEdges(actual->parent(),lastFormal->parent(),
                                 ETYPE_COPY,CQ_DN,size,edgeList);
      if (added)
        delta.add(edgeList);
    }
  }

  PU &calleePU = Pu_Table[ST_pu(calleeST)];

  if (PU_has_attr_malloc(calleePU) && cs->checkFlags(CS_FLAGS_HEAP_MODELED))
    return;

  // If the callee is a malloc wrapper, create a heap CGNode and add
  // it to the points to set of the actual return. The formal and actual
  // return nodes are not connected.
  // NOTE:  Due to conservative treatment of the call graph it is possible
  // for us to believe this routine calls a malloc wrapper when it in
  // fact does not.  If the callsite does not have a returnId(), then the
  // callee cannot be returning new memory through the return value of this
  // callsite, so we skip this optimization.  Can we assert that since there
  // is not return that we should not be connecting this call edge?
  if (PU_has_attr_malloc(calleePU) && cs->returnId() != 0) {
    FmtAssert(cs->returnId() != 0, ("No return id for malloc wrapper"));
    // Get the type of the actual return
    ConstraintGraphNode *actualRet = cgNode(cs->returnId())->parent();
    TY &ret_type = Ty_Table[actualRet->stInfo()->ty_idx()];
    TY_IDX heap_ty_idx;
    TY_IDX copy_ty_idx;
    if (TY_kind(ret_type) == KIND_POINTER)
      heap_ty_idx = TY_pointed(ret_type);
    else if (copyTarget(actualRet, copy_ty_idx) &&
             TY_kind(Ty_Table[copy_ty_idx]) == KIND_POINTER)
      heap_ty_idx = TY_pointed(Ty_Table[copy_ty_idx]);
    else {
      //FmtAssert(FALSE, ("**** Expecting KIND_POINTER *****"));
      heap_ty_idx = MTYPE_To_TY(Pointer_type);
    }

    TY &newRetType = Ty_Table[actualRet->ty_idx()];
    if (TY_kind(newRetType) == KIND_POINTER) {
      TY_IDX new_heap_ty_idx = TY_pointed(newRetType);
      if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
        fprintf(stderr,"CONNECT: New ty_idx %d, size %d - old ty_idx %d, "
                "size %d\n", new_heap_ty_idx,(UINT32)TY_size(new_heap_ty_idx),
                heap_ty_idx,(UINT32)TY_size(heap_ty_idx));
      heap_ty_idx = new_heap_ty_idx;
    }
    else {
      if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG))
        fprintf(stderr,"CONNECT: New ty_idx is not a pointer!\n");
    }

    CG_ST_IDX new_cg_st_idx = buildLocalStInfo(heap_ty_idx);
    ConstraintGraphNode *heapCGNode = getCGNode(new_cg_st_idx, 0);
    heapCGNode->stInfo()->addFlags(CG_ST_FLAGS_HEAP);
    actualRet->addPointsTo(heapCGNode, CQ_HZ);
    if (Get_Trace(TP_ALIAS,NYSTROM_SOLVER_FLAG)) {
      fprintf(stderr, "Adding heap due to callee: %s to caller: %s\n",
              ST_name(calleeST), ST_name(_ipaNode->Func_ST()));
      heapCGNode->print(stderr);
      heapCGNode->stInfo()->print(stderr);
      fprintf(stderr, "*** Actual return %d now points to %d\n",
              actualRet->id(),heapCGNode->id());
    }
    // Add the in/out edges to the edge delta
    CGEdgeSetIterator iter = actualRet->outCopySkewEdges().begin();
    for (; iter != actualRet->outCopySkewEdges().end(); iter++)
      delta.add(*iter);
    iter = actualRet->outLoadStoreEdges().begin();
    for (; iter != actualRet->outLoadStoreEdges().end(); iter++) {
      if ((*iter)->edgeType() == ETYPE_LOAD)
        delta.add(*iter);
    }
    iter = actualRet->inLoadStoreEdges().begin();
    for (; iter != actualRet->inLoadStoreEdges().end(); iter++) {
      if ((*iter)->edgeType() == ETYPE_STORE)
        delta.add(*iter);
    }
    return;
  }

  // Now connect the formal returns in callee to actual returns of caller
  list<CGNodeId>::const_iterator retIter = callee->returns().begin();
  ConstraintGraphNode *actualRet = cgNode(cs->returnId());
  if (actualRet && !actualRet->checkFlags(CG_NODE_FLAGS_NOT_POINTER)) {
    for (; retIter != callee->returns().end(); ++retIter) {
      ConstraintGraphNode *formalRet = cgNode(*retIter);
      if (formalRet->checkFlags(CG_NODE_FLAGS_NOT_POINTER))
        continue;
      INT64 size = actualRet->stInfo()->varSize();
      bool added = false;
      list<ConstraintGraphEdge *> edgeList;
      added = addPtrAlignedEdges(formalRet->parent(),actualRet->parent(),
                                 ETYPE_COPY,CQ_UP,size,edgeList);
      if (added)
        delta.add(edgeList);
    }
  }
}

void
ConstraintGraph::updateSummaryCallSiteId(SUMMARY_CALLSITE &summCallSite)
{
  UINT32 oldCSid = summCallSite.Get_constraint_graph_callsite_id();
  if (oldCSid != 0) {
    CallSite *cs = findUniqueCallSite(oldCSid);
    FmtAssert(cs != NULL, ("call site: %d not mapped", oldCSid));
    UINT32 newCSid = cs->id();
    summCallSite.Set_constraint_graph_callsite_id(newCSid);
  }
}

void
IPA_NystromAliasAnalyzer::updateLocalSyms(IPA_NODE *node)
{
  ConstraintGraph *cg = this->cg(node->Node_Index());

  if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
    fprintf(stderr, "updateLocalSyms: %s\n", cg->name());

  CGStInfoMap &cgStInfoMap = cg->stInfoMap();
  CGNodeToIdMap &cgNodeToIdMap = cg->nodeToIdMap();

  // Remove the old StInfos
  for (CGStInfoMapIterator iter = cg->newLocalStInfos().begin(); 
       iter != cg->newLocalStInfos().end(); iter++)  
  {
    StInfo *stInfo = iter->second;
    CG_ST_IDX old_cg_st_idx = iter->first;
    FmtAssert(cgStInfoMap.find(old_cg_st_idx) != cgStInfoMap.end(),
              ("Could not find old_cg_st_idx"));
    cgStInfoMap.erase(cgStInfoMap.find(old_cg_st_idx));
    // Remove all CGNodes that could be associated with the StInfo
    // We have to walk the map here, since after a collapsing the collapsed
    // nodes will not be in the offset list of the StInfo
    hash_set<CGNodeId> remNodes;
    for (CGNodeToIdMapIterator iter = cgNodeToIdMap.begin();
      iter != cgNodeToIdMap.end(); iter++) {
      ConstraintGraphNode *cgNode = iter->first;
      if (cgNode->cg_st_idx() != old_cg_st_idx)
        continue;
      remNodes.insert(cgNode->id());
    }
    for (hash_set<CGNodeId>::const_iterator iter = remNodes.begin();
       iter != remNodes.end(); iter++) {
      ConstraintGraphNode *cgNode = ConstraintGraph::cgNode(*iter);
      cgNodeToIdMap.erase(cgNodeToIdMap.find(cgNode));
    }
  }

  // For the new StInfos that we have created, create local symbols
  for (CGStInfoMapIterator iter = cg->newLocalStInfos().begin(); 
       iter != cg->newLocalStInfos().end(); iter++)  
  {
    StInfo *stInfo = iter->second;
    // Create the new symbol
    ST *st = Gen_Temp_Named_Symbol(stInfo->ty_idx(), "_cgIPAsym", 
                                   CLASS_VAR, SCLASS_AUTO);
    ST_IDX new_st_idx = ST_st_idx(st);
    // Fix the cg_st_idx with the st_idx of the new local symbol
    CG_ST_IDX new_cg_st_idx = ConstraintGraph::adjustCGstIdx(node, new_st_idx);
    cg->mapStInfo(stInfo, iter->first, new_cg_st_idx);
    ConstraintGraphNode *cgNode = stInfo->firstOffset();
    while (cgNode) {
      cg->remapCGNode(cgNode, new_cg_st_idx);
      cgNode = cgNode->nextOffset();
    }  
  }
}

void
IPA_NystromAliasAnalyzer::mapWNToUniqCallSiteCGNodeId(IPA_NODE *node)
{
  WN *entryWN = node->Whirl_Tree();
  FmtAssert(entryWN != NULL, ("Null WN tree!\n"));

  ConstraintGraph *cg = this->cg(node->Node_Index());

  // fprintf(stderr, "mapWNToUniqCGNodeId: %s\n", cg->name());

  // Remap the WNs to its unique CG nodes ids.
  for (WN_ITER *wni = WN_WALK_TreeIter(entryWN);
      wni; wni = WN_WALK_TreeNext(wni))
  {
    WN *wn = WN_ITER_wn(wni);
    const OPCODE   opc = WN_opcode(wn);

    UINT32 id = IPA_WN_MAP32_Get(Current_Map_Tab, WN_MAP_ALIAS_CGNODE, wn);

    if (id == 0)
      continue;

    if (OPCODE_is_call(opc)) {
      CallSiteId newCallSiteId = cg->findUniqueCallSite(id)->id();
      // fprintf(stderr, " Mapping WN (call) from old id:%d to new id:%d\n", 
      //         id, newCallSiteId);
      WN_MAP_CallSiteId_Set(wn, newCallSiteId);
    } else {
      CGNodeId newId = cg->findUniqueNode(id)->id();
      // fprintf(stderr, " Mapping WN from old id:%d to new id:%d\n", 
      //         id, newId);
      WN_MAP_CGNodeId_Set(wn, newId);
    }
  }
}

void
ConstraintGraph::updateCallSiteForBE(CallSite *cs)
{
  FmtAssert(callSite(cs->id()) == NULL, ("Callsite already exists\n"));
  _callSiteMap[cs->id()] = cs;
  // fprintf(stderr, " Mapping callsite %d\n", cs->id());
}

static void 
mapStInfoAndNodesLocally(StInfo *stInfo, CG_ST_IDX cg_st_idx,
                         ConstraintGraph *localCG)
{
  localCG->mapStInfo(stInfo, cg_st_idx, cg_st_idx);
  ConstraintGraphNode *node = stInfo->firstOffset();
  while (node) {
    FmtAssert(localCG->checkCGNode(cg_st_idx, node->offset()) == NULL,
              ("node: %d not expected in localCG", node->id()));
    localCG->mapCGNode(node);
    node = node->nextOffset();
  }
}

void
IPA_NystromAliasAnalyzer::updateCGForBE(IPA_NODE *ipaNode)
{
  ConstraintGraph *localCG = this->cg(ipaNode->Node_Index());

  if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
    fprintf(stderr, "updateCGForBE: %s\n", localCG->name());

  WN *entryWN = ipaNode->Whirl_Tree();
  for (WN_ITER *wni = WN_WALK_TreeIter(entryWN); wni; 
       wni = WN_WALK_TreeNext(wni))
  {
    WN *wn = WN_ITER_wn(wni);
    const OPCODE opc = WN_opcode(wn);

    // Get the symbol from the WN and add StInfos/CGNodes associated
    // with the symbol
    OPCODE op = WN_opcode(wn);
    ST_IDX st_idx = WN_st_idx(wn);
    if (OPCODE_has_sym(op) && st_idx != 0) 
    {
      // Global not-preg symbol
      if (ST_IDX_level(st_idx) == GLOBAL_SYMTAB &&
          ST_class(&St_Table[st_idx]) != CLASS_PREG) 
      {
        CG_ST_IDX cg_st_idx = st_idx;
        StInfo *globalStInfo = ConstraintGraph::globalCG()->stInfo(cg_st_idx);
        if (globalStInfo) {
          StInfo *globalStInfoCopy = localCG->stInfo(cg_st_idx);
          if (globalStInfoCopy) {
            FmtAssert(globalStInfoCopy == globalStInfo,
                      ("Expecting global StInfos to be the same"));
          } else {
            // Map the StInfo and all nodes to the localCG
            if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
              fprintf(stderr, " updateCGForBE global: %s\n",
                      ST_name(cg_st_idx));
            mapStInfoAndNodesLocally(globalStInfo, cg_st_idx, localCG);
          }
        }
      }
      else if (ST_IDX_level(st_idx) != GLOBAL_SYMTAB)
      {
        // Local symbols are expected to be in the localCG
        CG_ST_IDX cg_st_idx = ConstraintGraph::adjustCGstIdx(ipaNode, st_idx);
        if (localCG->stInfo(cg_st_idx) == NULL) {
          if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
            fprintf(stderr, "Found local symbol %s with no StInfo mapping"
                    " in local CG: %s\n", ST_name(St_Table[st_idx]),
                    localCG->name());
        }
        // fdump_tree(stderr, entryWN);
        // FmtAssert(localCG->stInfo(cg_st_idx) != NULL, 
        //         ("Expecting local symbol to be already present in localCG"));
      }
    }
                  
    UINT32 id = IPA_WN_MAP32_Get(ipaNode->Map_Table(), WN_MAP_ALIAS_CGNODE, wn);

    if (id == 0)
      continue;

    if (OPCODE_is_call(opc)) {
      // Check if the call site is local to this PU
      CallSite *cs = localCG->callSite(id);
      if (cs == NULL) {
        // Query the unique id to call site map
        cs = ConstraintGraph::uniqueCallSite(id);
        FmtAssert(cs != NULL, ("Callsite: %d not mapped", id));
        // Add this call site to the local CG
        localCG->updateCallSiteForBE(cs);
      }
    } else {
      ConstraintGraphNode *uniqNode = ConstraintGraph::cgNode(id);
      FmtAssert(uniqNode != NULL, ("CGNodeId: %d not mapped", id));
      // Check if the node mapped to the WN belongs to the global CG
      ConstraintGraph *remoteCG = uniqNode->cg();
      if (remoteCG == ConstraintGraph::globalCG()) 
      {
        // Check if it is already mapped to the localCG
        if (! localCG->checkCGNode(uniqNode->cg_st_idx(), uniqNode->offset()) ) 
        {
          if (uniqNode->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
            if (localCG->stInfo(uniqNode->cg_st_idx()) == NULL)
              mapStInfoAndNodesLocally(uniqNode->stInfo(),
                                       uniqNode->cg_st_idx(),
                                       localCG);
            // The collapsed node may/may not be in the StInfo list.
            // So check and add 
            if (! localCG->checkCGNode(uniqNode->cg_st_idx(),
                                       uniqNode->offset()) )
              localCG->mapCGNode(uniqNode);
            continue;
          }
          // Add the StInfo and all the nodes
          CG_ST_IDX cg_st_idx = uniqNode->cg_st_idx();
          StInfo *globalStInfo = uniqNode->stInfo();
          FmtAssert(localCG->stInfo(cg_st_idx) == NULL,
                    ("Not expecting StInfo"));
          if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
            fprintf(stderr, " updateCGForBE global: %s\n", ST_name(cg_st_idx));
          mapStInfoAndNodesLocally(globalStInfo, cg_st_idx, localCG);
          // This node should have been added by now
          FmtAssert(localCG->checkCGNode(uniqNode->cg_st_idx(),
                                         uniqNode->offset()) != NULL,
                    ("Node should have been mapped"));
        }
      } 
      else if (remoteCG != localCG)
      {
        // The node mapped to the WN belongs to a remote non-global CG
        // Check if its a PREG
        StInfo *remoteStInfo = uniqNode->stInfo();
        if (remoteStInfo->checkFlags(CG_ST_FLAGS_PREG)) 
        {
          CG_ST_IDX cg_st_idx = uniqNode->cg_st_idx();
          // PREGs will retain their remote CGs unique file/pu idx
          // Check if it is already mapped to the localCG
          if (! localCG->checkCGNode(cg_st_idx, uniqNode->offset()) ) {
            if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
              fprintf(stderr, " updateCGForBE preg node: %d\n", uniqNode->id());
            if (! localCG->stInfo(cg_st_idx) )
              localCG->mapStInfo(remoteStInfo, cg_st_idx, cg_st_idx);
            localCG->mapCGNode(uniqNode);
          }
        }
        else
        {
          // Its a local symbol of a remote CG
          // Check if the node exists locally
          bool found = false;
          for (CGNodeToIdMapIterator iter = localCG->lBegin();
               iter != localCG->lEnd(); iter++) {
            if (iter->second == uniqNode->id()) {
              found = true;
              break;
            }
          }
          if (!found) {
            if (uniqNode->checkFlags(CG_NODE_FLAGS_COLLAPSED)) {
              if (localCG->stInfo(uniqNode->cg_st_idx()) == NULL)
                mapStInfoAndNodesLocally(uniqNode->stInfo(), 
                                         uniqNode->cg_st_idx(),
                                         localCG);
              // The collapsed node may/may not be in the StInfo list.
              // So check and add 
              if (! localCG->checkCGNode(uniqNode->cg_st_idx(),
                                         uniqNode->offset()) )
                localCG->mapCGNode(uniqNode);
              continue;
            }
            // Remap the remote StInfo and its associated nodes to the localCG
            // using the remote pu/file idx   
            StInfo *remoteStInfo = uniqNode->stInfo();
            FmtAssert(localCG->stInfo(uniqNode->cg_st_idx()) == NULL,
                      ("remoteStInfo not expected in localCG"));
            
            if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG)) {
              char buf[128];
              fprintf(stderr, " updateCGForBE remotelocal st %s\n",
                      ConstraintGraph::printCGStIdx(uniqNode->cg_st_idx(),
                                                    buf, 128));
            }
            mapStInfoAndNodesLocally(remoteStInfo,
                                     uniqNode->cg_st_idx(),
                                     localCG);
          }
        }
      } 
      else 
      {
        // We expect the node to be in this local CG
        FmtAssert(localCG->checkCGNode(uniqNode->cg_st_idx(),
                                       uniqNode->offset()),
                  ("Expecting node: %d to be in localCG: %s\n",
                   uniqNode->id(), localCG->name()));
      }
    }
  }
}

void 
ConstraintGraph::cloneConstraintGraphMaps(IPA_NODE *caller, IPA_NODE *callee)
{
  ConstraintGraph *callerCG = 
            IPA_NystromAliasAnalyzer::aliasAnalyzer()->cg(caller->Node_Index());
  ConstraintGraph *calleeCG = 
            IPA_NystromAliasAnalyzer::aliasAnalyzer()->cg(callee->Node_Index());

  if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
    fprintf(stderr, "cloneConstraintGraphMaps: caller: %s callee: %s\n",
            callerCG->name(), calleeCG->name());

  for (hash_map<ST_IDX, ST_IDX>::iterator iter = origToCloneStIdxMap.begin(); 
       iter != origToCloneStIdxMap.end(); iter++) 
  {
    ST_IDX orig_st_idx  = iter->first;
    ST_IDX clone_st_idx = iter->second;

    // fprintf(stderr, "orig_st_idx: %d clone_st_idx: %d\n",
    //        orig_st_idx, clone_st_idx);

    // Clone the StInfo and all its CGNodes
    CG_ST_IDX orig_cg_st_idx = 
              ConstraintGraph::adjustCGstIdx(callee, orig_st_idx);
    StInfo *origStInfo = calleeCG->stInfo(orig_cg_st_idx);
    // FmtAssert(origStInfo != NULL , ("Expecting original StInfo in callee"));
    // If we haven't seen this symbol before, we may not have an StInfo
    // in the callee
    if (origStInfo == NULL)
      continue;

    CG_ST_IDX clone_cg_st_idx = 
              ConstraintGraph::adjustCGstIdx(caller, clone_st_idx);
    StInfo *cloneStInfo = callerCG->stInfo(clone_cg_st_idx);
    // if StInfo does not exist, add
    if (cloneStInfo == NULL) {
      // Map the StInfo
      callerCG->mapStInfo(origStInfo, orig_cg_st_idx, clone_cg_st_idx);
      ConstraintGraphNode *orig_node = origStInfo->firstOffset();
      while (orig_node) {
        callerCG->cloneCGNode(orig_node, clone_cg_st_idx);
        orig_node = orig_node->nextOffset(); 
      }
    }
  }
  origToCloneStIdxMap.clear();
}

void
ConstraintGraph::promoteLocals(IPA_NODE *callee) {

  ConstraintGraph *calleeCG = 
            IPA_NystromAliasAnalyzer::aliasAnalyzer()->cg(callee->Node_Index());

  ConstraintGraph *globalCG = ConstraintGraph::globalCG();

  if (Get_Trace(TP_ALIAS, NYSTROM_CG_BE_MAP_FLAG))
    fprintf(stderr, "promote local to globals in %s\n", calleeCG->name());

  for (hash_map<ST_IDX, ST_IDX>::iterator iter = promoteStIdxMap.begin(); 
       iter != promoteStIdxMap.end(); iter++)
  {

    ST_IDX orig_st_idx  = iter->first;
    ST_IDX globl_st_idx = iter->second;

    // assert orig must be local static.
    // fprintf(stderr, "orig_st_idx: %d globl_st_idx: %d\n",   orig_st_idx, globl_st_idx);

        // Clone the StInfo and all its CGNodes
    CG_ST_IDX orig_cg_st_idx = 
              ConstraintGraph::adjustCGstIdx(callee, orig_st_idx);
    StInfo *origStInfo = calleeCG->stInfo(orig_cg_st_idx);
    // FmtAssert(origStInfo != NULL , ("Expecting original StInfo in callee"));
    // If we haven't seen this symbol before, we may not have an StInfo
    // in the callee
    if (origStInfo == NULL)
      continue;

    // origStInfo->print(stderr, TRUE);
    StInfo *globalStInfo = globalCG->stInfo(globl_st_idx);
    // if StInfo does not exist, add
    Is_True(globalStInfo == NULL, ("global must havs not added\n"));
    // Map the StInfo
    globalCG->mapStInfo(origStInfo, orig_cg_st_idx, globl_st_idx);
    ConstraintGraphNode *orig_node = origStInfo->firstOffset();
    // extract nodes from local cg to global.
    while (orig_node) {
      calleeCG->_cgNodeToIdMap.erase(orig_node);
      orig_node->cg_st_idx(globl_st_idx);
      orig_node->cg(globalCG);
      globalCG->_cgNodeToIdMap[orig_node] = orig_node->id();
      orig_node = orig_node->nextOffset(); 
    }
    // update stinfo be global. delete stinfo in callee
    origStInfo->addFlags(CG_ST_FLAGS_GLOBAL);
    calleeCG->stInfoMap().erase(orig_cg_st_idx);
  } 
  promoteStIdxMap.clear();
}


void
dbgPrintCGStIdx(CG_ST_IDX cg_st_idx)
{
  fprintf(stderr, "<file:%d pu:%d st_idx:%d>",
          FILE_NUM_ST_IDX(cg_st_idx),
          PU_NUM_ST_IDX(cg_st_idx),
          SYM_ST_IDX(cg_st_idx));
}
