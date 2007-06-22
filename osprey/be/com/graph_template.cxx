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

/**
*** Module: graph_template.cxx
*** $Revision$
*** $Date$
*** $Author$
*** $Source$
***
*** Revision history:
***
***     10-NOV-94 dkchen - Original Version
***
*** Description:
***
*** This file contains definitions for fuctions in the DIRECTED_GRAPH16.
*** The set of vertices and edges are implemented with dynamic arrays.
*** The index 0 to these array is reserved as NULL pointer, which is used
*** to represent the end of the free vertex/edge list as well as the end
*** of the in/out edge-lists for a vertex. Therefore, when we have _vcnt
*** vertices, the _v array has _vcnt+1 elements.
***
*** Besides, we require that initial vertices and edges counts be given
*** to the constructor.
***
**/


#include "cxx_graph.h"
#include "graph_template.h"

#if 0

template <class EDGE_TYPE, class VERTEX_TYPE>
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::
DIRECTED_GRAPH16( const VINDEX16 vsize, const EINDEX16 esize) {

  //Is_True( vsize >=0, ("Illegal graph size\n"));

  _vmpool = CXX_NEW(MEM_POOL,Malloc_Mem_Pool);
  MEM_POOL_Initialize(_vmpool,"vmpool",FALSE);
  MEM_POOL_Push(_vmpool);
  _v.Set_Mem_Pool(_vmpool);
  _v.Alloc_array(vsize+1);
  _v.Setidx( 0 );
  _vcnt = 0;
  _vfree = 0;

  //for (mINT16 i=1; i<=_vcnt; i++) {	// initialization
  //  _v[i].Set_Out_Edge(0);
  //  _v[i].Set_In_Edge(0);
  //}

  _empool = CXX_NEW(MEM_POOL,Malloc_Mem_Pool);
  MEM_POOL_Initialize(_empool,"empool",FALSE);
  MEM_POOL_Push(_empool);
  _e.Set_Mem_Pool(_empool);
  _e.Alloc_array(esize+1);
  _e.Setidx( 0 );
  _ecnt = 0;
  _efree = 0;

}

template <class EDGE_TYPE, class VERTEX_TYPE>
VINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Add_Vertex() {
  VINDEX16 new_vertex;

  // Is_True(_vcnt < GRAPH16_CAPACITY, ("Too many vertices\n"));
  if (_vcnt == GRAPH16_CAPACITY) return 0;

  if (_vfree == 0) { // grow the _v[] to accept more vertices
    new_vertex = _v.Newidx();
  } else {
    new_vertex = _vfree;
    _vfree = _v[_vfree].Get_Next_Free_Vertex();
  }

  // reset the in/out edge-lists to NULL
  _v[new_vertex].Set_Out_Edge(0);
  _v[new_vertex].Set_In_Edge(0);

  _vcnt++;

  return new_vertex;

}

template <class EDGE_TYPE, class VERTEX_TYPE>
EINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Add_Edge(VINDEX16 from, VINDEX16 to) {
  EINDEX16 new_edge;

  // Is_True(_ecnt < GRAPH16_CAPACITY, ("Too many edges\n"));
  if (_ecnt == GRAPH16_CAPACITY) return 0;
  if (_efree == 0) { // grow the _e[] to accept more edges
    new_edge = _e.Newidx();
  } else {
    new_edge = _efree;
    _efree = _e[_efree].Get_Next_Free_Edge();
  }
  
  _e[new_edge].Set_Source(from);
  _e[new_edge].Set_Sink(to);

  _ecnt++;

  // insert this edge into the out-edge list of the source vertex 'from'
  _e[new_edge].Set_Next_Out_Edge(_v[from].Get_Out_Edge());
  _v[from].Set_Out_Edge(new_edge);

  // insert this edge into the in-edge list of the sink vertex 'to'
  _e[new_edge].Set_Next_In_Edge(_v[to].Get_In_Edge());
  _v[to].Set_In_Edge(new_edge);

  return new_edge;

}

template <class EDGE_TYPE, class VERTEX_TYPE>
EINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Add_Unique_Edge(VINDEX16 from, VINDEX16 to) {
  EINDEX16 new_edge;

  // see if an edge already exists. No multiple edges between the same
  // pair of source and sink vertices.
  if (new_edge=Get_Edge(from,to)) return new_edge;

  return Add_Edge(from,to);

}

template <class EDGE_TYPE, class VERTEX_TYPE>
EINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Get_Edge(VINDEX16 from, VINDEX16 to) const {
  EINDEX16 e;
  e = _v[from].Get_Out_Edge();
  while (e != 0) {
    if (_e[e].Get_Sink() == to) return e;
    e = _e[e].Get_Next_Out_Edge();
  }
  return 0; // no corresponding edge
}

template <class EDGE_TYPE, class VERTEX_TYPE>
void
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Delete_Vertex(VINDEX16 v) {

  EINDEX16 e;

  // see if the vertex exists
  Is_True (Vertex_Is_In_Graph(v), ("Vertex not in graph\n"));

  while (e = _v[v].Get_In_Edge()) {
    Delete_Edge(e);
  };
  while (e = _v[v].Get_Out_Edge()) {
    Delete_Edge(e);
  };

  // insert the deleted vertex in free list
  _v[v].Set_Next_Free_Vertex(_vfree);
  _v[v].Set_To_Free();
  _vfree = v;

  _vcnt--;
  
}

template <class EDGE_TYPE, class VERTEX_TYPE>
void
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Delete_Edge(EINDEX16 e) {

  VINDEX16 from, to;
  EINDEX16 e1;

  // see if the edge exists
  Is_True (Edge_Is_In_Graph(e), ("Edge not in graph\n"));

  from = _e[e].Get_Source();
  to = _e[e].Get_Sink();

  if (_v[from].Get_Out_Edge() == e) {  // to delete the first edge in list
    _v[from].Set_Out_Edge(_e[e].Get_Next_Out_Edge());
  } else {
    e1 = _v[from].Get_Out_Edge();
    while (_e[e1].Get_Next_Out_Edge() != e) 
	e1 = _e[e1].Get_Next_Out_Edge();
    _e[e1].Set_Next_Out_Edge(_e[e].Get_Next_Out_Edge());
  }

  if (_v[to].Get_In_Edge() == e) {  // to delete the first edge in list
    _v[to].Set_In_Edge(_e[e].Get_Next_In_Edge());
  } else {
    e1 = _v[to].Get_In_Edge();
    while (_e[e1].Get_Next_In_Edge() != e) 
	e1 = _e[e1].Get_Next_In_Edge();
    _e[e1].Set_Next_In_Edge(_e[e].Get_Next_In_Edge());
  }

  _e[e].Set_Next_Free_Edge(_efree);  // insert the deleted edge in free list
  _e[e].Set_To_Free();
  _efree = e;

  _ecnt--;
  
}

template <class EDGE_TYPE, class VERTEX_TYPE>
VINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Get_Vertex() const {
  VINDEX16 v;

  if (Is_Empty()) return 0;

  v = _v.Lastidx();
  while (v>0 &&_v[v].Is_Free() ) v--;	// skip over free vertices
  Is_True( v>0 , ("Fail to get vertex\n"));

  return v;
}

template <class EDGE_TYPE, class VERTEX_TYPE>
VINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Get_Next_Vertex(VINDEX16 v) const {

  Is_True( Vertex_Is_In_Graph(v), ("Vertex does not exist in graph\n"));

  do {
    v--;
  } while (v>0 && _v[v].Is_Free() );	// skip over free vertices

  return v;
}

template <class EDGE_TYPE, class VERTEX_TYPE>
EINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Get_Edge() const {
  EINDEX16 e;

  if (_ecnt == 0) return 0;

  e = _e.Lastidx();
  while (_e[e].Is_Free() && e>0) e--;	// skip over free edges
  Is_True( e>0 , ("Fail to get edge\n"));

  return e;
}

template <class EDGE_TYPE, class VERTEX_TYPE>
EINDEX16
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Get_Next_Edge(EINDEX16 e) const {

  Is_True( Edge_Is_In_Graph(e), ("Edge does not exist in graph\n"));

  do {
    e--;
  } while (e > 0 && _e[e].Is_Free() );	// skip over free edges

  return e;
}

template <class EDGE_TYPE, class VERTEX_TYPE>
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>&
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::
operator=(const DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>& g) {

  // copy everything except mpool

  _vfree = g._vfree;
  _vcnt = g._vcnt;
  _efree = g._efree;
  _ecnt = g._ecnt;

  _v = g._v;
  _e = g._e;

  return *this;
}

template <class EDGE_TYPE, class VERTEX_TYPE>
void
DIRECTED_GRAPH16<EDGE_TYPE,VERTEX_TYPE>::Print(FILE *file) const {
  VINDEX16 i;
  EINDEX16 e;

  fprintf(file,"Print out graph edges and vertices ...\n");
  for (i=1; i<_e.Lastidx()+1; i++)
   if (!_e[i].Is_Free())
    fprintf(file, "%d: %d --> %d\n", i, _e[i]._from, _e[i]._to);

  for (i=1; i<_v.Lastidx()+1; i++)
   if (!_v[i].Is_Free()) {
    fprintf(file, "( ");
    e = _v[i].Get_In_Edge();
    while (e) {
      fprintf(file, "%d ", e);
      e = _e[e].Get_Next_In_Edge();
    }
    fprintf(file, ") %d ( ", i);
    e = _v[i].Get_Out_Edge();
    while (e) {
      fprintf(file, "%d ", e);
      e = _e[e].Get_Next_Out_Edge();
    }
    fprintf(file, ")\n");
  }
  
}

#endif
