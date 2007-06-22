/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

/* A local instruction scheduler dedicated to opteron.
   May 29, 2003
*/

#include "cgir.h"
#include "glob.h"
#include "tn_map.h"
#include "cgtarget.h"
#include "cg_vector.h"
#include "gra_live.h"
#include "freq.h"
#include "ti_res.h"
#include "register.h"
#include "tracing.h"
#include "config_asm.h"
#include "note.h"
#include "cgexp.h"
#include "lra.h"
#include "wn_util.h"
#include "hb_hazards.h"
#include "reg_live.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>

#include "cg_sched.h"

enum ICU { NONE = 0, ALU, AGU, FADD, FMUL, FMISC };

static unsigned int BBs_Processed = 0;

static ICU TOP_2_Res[TOP_count];

static uint16_t sched_order = 0;

typedef struct {
  TN*      mem_base;
  TN*      mem_index;
  ST*      mem_sym;
  uint64_t mem_ofst;

  uint16_t uses;
  uint16_t latency;

  uint16_t sched_order;
  uint16_t pred_order;  // the order of the last scheduled predecessor
  int16_t release_time; // Aka the dispatch time. Determined by data dependences.
  int16_t issue_time;   // Determined dynamically by the current hw resources.
  int16_t deadline;

  uint16_t num_succs;
  uint16_t num_preds;

  bool is_scheduled;
} OPR;

static OPR* opr_array = NULL;

#define ASSERT(c)   FmtAssert( c, ("KEY_SCH error") );

#define Get_OPR(op)               (&opr_array[OP_map_idx(op)])
#define OPR_release_time(o)       ((o)->release_time)
#define OPR_deadline(o)           ((o)->deadline)
#define OPR_issue_time(o)         ((o)->issue_time)
#define OPR_is_scheduled(o)       ((o)->is_scheduled == true)
#define Set_OPR_is_scheduled(o)   ((o)->is_scheduled = true)
#define OPR_num_preds(o)          ((o)->num_preds)
#define OPR_num_succs(o)          ((o)->num_succs)
#define OPR_sched_order(o)        ((o)->sched_order)
#define OPR_pred_order(o)         ((o)->pred_order)
#define OPR_mem_base(o)           ((o)->mem_base)
#define OPR_mem_index(o)          ((o)->mem_index)
#define OPR_mem_ofst(o)           ((o)->mem_ofst)
#define OPR_mem_sym(o)            ((o)->mem_sym)
#define OPR_uses(o)               ((o)->uses)
#define OPR_latency(o)            ((o)->latency)

static const int num_fu[] = {
  0,   /* NONE */
  3,   /* ALU  */
  3,   /* AGU  */
  1,   /* FADD */
  1,   /* FMUL */
  1,   /* FMISC */
};


class MRT {
private:
  BOOL trace;

  REGISTER_SET live_in[ISA_REGISTER_CLASS_MAX+1];
  REGISTER_SET live_out[ISA_REGISTER_CLASS_MAX+1];
  REGISTER_SET avail_regs[ISA_REGISTER_CLASS_MAX+1];

  static const int mem_ops_rate = 2;

  struct Resource_Table_Entry {
    uint8_t decoded_ops;  /* the number of decoded ops. */
    uint8_t mem_ops;    /* how many of <decoded_ops> are mem ops. */
    uint8_t fp_ops;     /* how many of <decoded_ops> are fp ops.  */
    bool resources[6][3];
  };

  void Init_Table_Entry( Resource_Table_Entry* e )
  {
    e->decoded_ops = e->mem_ops = e->fp_ops = 0;

    bzero( e->resources, sizeof(e->resources) );
    for( int i = NONE; i <= FMISC; i++ ){
      for( int j = 0; j < num_fu[i]; j++ ){
	e->resources[i][j] = true;
      }
    }
  }

  bool Probe_Resources( int cycle, OP* op, int, bool take_it );
  int Get_Dispatch_Unit( OP*, int );

  bool TOP_is_convert( const TOP top )
  {
    return ( top == TOP_cvttss2si  || top == TOP_cvttsd2si  ||
	     top == TOP_cvttss2siq || top == TOP_cvttsd2siq ||
	     top == TOP_cvtsi2sd   || top == TOP_cvtsi2ss   ||
	     top == TOP_cvtsi2sdq  || top == TOP_cvtsi2ssq  ||
	     top == TOP_cvtss2sd   || top == TOP_cvtsd2ss );
  }

  bool TOP_is_lea( const TOP top )
  {
    return ( top == TOP_lea32   || top == TOP_lea64  ||
	     top == TOP_leax32  || top == TOP_leax64 ||
	     top == TOP_leaxx32 || top == TOP_leaxx64 );
  }

  int entries;
  Resource_Table_Entry** Resource_Table;

public:
  MRT() {};
  ~MRT() {};

  static const int issue_rate = 3;

  void Init( BB*, int, BOOL, MEM_POOL* );
  void Reserve_Resources( OP*, int );
  void Set_Completion_Time( OP* );
  void Compute_Issue_Time( OP*, int );
  bool Decoder_is_Saturated( int c )
  {
    return Resource_Table[c]->decoded_ops == issue_rate;
  }

  bool Memory_Saturated( int cycle )
  {
    return Resource_Table[cycle]->mem_ops >= mem_ops_rate;
  }

  int  Decoded_Ops( int c ) { return Resource_Table[c]->decoded_ops; }
};

static MRT mrt;


static void Print_Register_Set( char* name, REGISTER_SET reg_set, ISA_REGISTER_CLASS cl )
{
  if( REGISTER_SET_EmptyP( reg_set ) )
    return;

  fprintf( TFile, "%s: \n\t", name );
  REGISTER_SET_Print( reg_set, TFile );
  fprintf( TFile, "\n\t(Registers):");

  REGISTER reg;
  FOR_ALL_REGISTER_SET_members( reg_set, reg) {
    fprintf( TFile, " %s", REGISTER_name(cl, reg) );
  }

  fprintf (TFile, "\n");
}


void MRT::Init( BB* bb, int size, BOOL trace, MEM_POOL* mem_pool )
{
  static bool TOP_2_Res_is_valid = false;

  entries = size;
  this->trace = trace;
  Resource_Table = (MRT::Resource_Table_Entry**)
    MEM_POOL_Alloc( mem_pool, (sizeof(Resource_Table[0]) * entries) );

  for( int i = 0; i < entries; i++ ){
    Resource_Table[i] = (MRT::Resource_Table_Entry*)
      MEM_POOL_Alloc( mem_pool, sizeof(Resource_Table[i][0]) );
    Init_Table_Entry( Resource_Table[i] );
  }

  // Setup the table for resource usage by each top.
  if( !TOP_2_Res_is_valid ){
    TOP_2_Res_is_valid = true;

    for( int i = 0; i < TOP_count; i++ ){
      const TOP top = (TOP)i;
      ICU res = NONE;

      if( TOP_is_load( top ) ){
	res = AGU;

      } else if( TOP_is_flop(top) ||
		 TOP_is_convert(top) ){
	res = FADD;

	if( TOP_is_fmul( top ) ||
	    TOP_is_sqrt( top ) ||
	    TOP_is_fdiv( top ) ){
	  res = FMUL;

	} else if( TOP_is_store( top ) ){
	  res = FMISC;
	}

      } else {
	res = ALU;

	if( TOP_is_store( top ) ||
	    TOP_is_lea( top ) ){
	  res = AGU;
	}
      }

      TOP_2_Res[top] = res;
    }
  }

  // Compute the registers usage.
  ISA_REGISTER_CLASS cl;
  FOR_ALL_ISA_REGISTER_CLASS( cl ){
    live_in[cl] = live_out[cl] = REGISTER_SET_EMPTY_SET;
    avail_regs[cl] = REGISTER_CLASS_allocatable(cl);

    for( REGISTER reg = REGISTER_MIN; reg <= REGISTER_MAX; reg++ ){
      if( REG_LIVE_Into_BB( cl, reg, bb ) )
	live_in[cl] = REGISTER_SET_Union1( live_in[cl], reg );

      if( REG_LIVE_Outof_BB( cl, reg, bb ) )
	live_out[cl] = REGISTER_SET_Union1( live_out[cl], reg );
    }

    avail_regs[cl] = REGISTER_SET_Difference( avail_regs[cl], live_in[cl] );
    avail_regs[cl] = REGISTER_SET_Difference( avail_regs[cl], live_out[cl] );

    if( trace ){
      Print_Register_Set( "live_in", live_in[cl], cl );
      Print_Register_Set( "live_out", live_out[cl], cl );
      Print_Register_Set( "avail_regs", avail_regs[cl], cl );
    }
  }
}


int MRT::Get_Dispatch_Unit( OP* op, int cycle )
{
  const ICU res = TOP_2_Res[OP_code(op)];

  if( res == FADD || res == FMUL || res == FMISC )
    return 0;

  return Resource_Table[cycle]->decoded_ops - Resource_Table[cycle]->fp_ops;
}


/* Compute the issue time of <op>, given that <op> is dispatched at
   cycle <clock>.
*/
void MRT::Compute_Issue_Time( OP* op, int clock )
{
  OPR* opr = Get_OPR( op );
  const int dispatch_unit = Get_Dispatch_Unit( op, clock );
  const ICU res = TOP_2_Res[OP_code(op)];

  if( clock < OPR_release_time( opr ) )
    clock = OPR_release_time( opr );

  while( clock <= OPR_deadline( opr ) ){
    if( Probe_Resources( clock, op, dispatch_unit, false ) )
      break;
    clock++;
  }

  ASSERT( clock <= OPR_deadline(opr) );
  OPR_issue_time(opr) = clock;
}


/* Check whether <op> can be scheduled at time <cycle> at <dispatch_unit>.
   Or check the availability of resoures[<cycle>][<dispatch_unit>].
*/
bool MRT::Probe_Resources( int cycle, OP* op, int dispatch_unit, bool take_it )
{
  const TOP top = OP_code(op);
  Resource_Table_Entry* entry = Resource_Table[cycle];

  if( OP_memory( op ) &&
      entry->mem_ops == mem_ops_rate )
    return false;

  if( OP_idiv(op) && entry->decoded_ops > 0 )
    return false;

  /* int mul only takes the first alu slot. */
  if( TOP_is_imul(top) && entry->decoded_ops > 0 )
    return false;

  const ICU res = TOP_2_Res[top];
  ASSERT( num_fu[res] > dispatch_unit );

  if( !entry->resources[res][dispatch_unit] )
    return false;

  if( TOP_is_lea(top) ){
    Resource_Table_Entry* entry1 = Resource_Table[cycle+1];
    if( !entry1->resources[ALU][dispatch_unit] )
      return false;
  }

  if( take_it ){
    entry->resources[res][dispatch_unit] = false;

    if( TOP_is_imul(top) ){
      //ASSERT( dispatch_unit == 0 );
      entry->resources[ALU][dispatch_unit] = false;

    } else if( TOP_is_lea(top) ){
      Resource_Table_Entry* entry1 = Resource_Table[cycle+1];
      ASSERT( entry1->resources[ALU][dispatch_unit] );
      entry1->resources[ALU][dispatch_unit] = false;
    }
  }

  return true;
}


/* <op> is dispatched at time <cycle>. But its won't be issued until
   its issue time due.
*/
void MRT::Reserve_Resources( OP* op, int cycle )
{
  OPR* opr = Get_OPR( op );
  const int dispatch_unit = Get_Dispatch_Unit( op, cycle );

  if( !Probe_Resources( OPR_issue_time(opr), op, dispatch_unit, true ) ){
    ASSERT( false );
  }

  // Update the decoding info.
  Resource_Table_Entry* entry = Resource_Table[cycle];
  const ICU res = TOP_2_Res[OP_code(op)];

  ASSERT( entry->decoded_ops < issue_rate );

  if( OP_idiv(op) )
    entry->decoded_ops = issue_rate;
  else
    entry->decoded_ops++;

  if( res == FADD || res == FMUL || res == FMISC )
    entry->fp_ops++;

  if( OP_memory( op ) )
    entry->mem_ops++;
}


void KEY_SCH::Build_Ready_Vector()
{
  OP* op;

  FOR_ALL_BB_OPs_FWD (bb, op) {
    OPR* opr = Get_OPR( op );
    if( OPR_num_preds(opr) == 0 ){
      VECTOR_Add_Element( _ready_vector, op );
    }
  }
}


void KEY_SCH::Init()
{
  OP* op = NULL;

  // Init schedule info.
  _ready_vector = VECTOR_Init( BB_length(bb), mem_pool );
  _sched_vector = VECTOR_Init( BB_length(bb), mem_pool );

  int max_indx = 0;
  FOR_ALL_BB_OPs( bb, op ){
    max_indx = std::max( max_indx, OP_map_idx(op) );
  }
  max_indx++;

  opr_array = (OPR*) MEM_POOL_Alloc( mem_pool,
				     sizeof(opr_array[0]) * max_indx );
  bzero( opr_array, ( sizeof(opr_array[0]) * max_indx ) );

  // Init resource table.
  int rtable_size = 0;
  int max_resource_cycles = 0;

  FOR_ALL_BB_OPs_FWD( bb, op ){
    INT cur_resource_cycles = TI_RES_Cycle_Count(OP_code(op));
    if (cur_resource_cycles > max_resource_cycles) {
      max_resource_cycles = cur_resource_cycles;
    }
    INT op_latency = cur_resource_cycles;
    for( ARC_LIST* arcs = OP_succs(op); arcs != NULL; arcs = ARC_LIST_rest(arcs)) {
      ARC *arc = ARC_LIST_first(arcs);
      if (ARC_latency(arc) > op_latency) {
	op_latency = ARC_latency(arc);
      }
    }
    rtable_size += op_latency;
  }

  // increase table size by the maximum number of resource cycles needed by
  // any OP.
  rtable_size += max_resource_cycles;
  _U = rtable_size;

  mrt.Init( bb, _U, trace, mem_pool );
}


void KEY_SCH::Reorder_BB()
{
  ASSERT( VECTOR_count(_sched_vector) == BB_length(bb) );
  BB_Remove_All( bb );

  for( int i = 0; i < VECTOR_count(_sched_vector); i++ ){
    OP* op = OP_VECTOR_element(_sched_vector, i);
    OPR* opr = Get_OPR( op );
    BB_Append_Op( bb, op );
  }
}


void KEY_SCH::Build_OPR()
{
  OP* op;

  int mem_ops = 0;
  _true_cp = _cp = 0;

  bzero( defop_by_reg, sizeof(defop_by_reg) );

  FOR_ALL_BB_OPs_FWD( bb, op ){
    OPR* opr = Get_OPR( op );
    OPR_release_time(opr) = 0;
    OPR_deadline(opr) = _U-1;
    OPR_sched_order(opr) = _U;
    OPR_pred_order(opr) = _U;

    OPR_uses(opr) = 0;
    OPR_latency(opr) = 0;

    for( ARC_LIST* arcs = OP_succs(op); arcs != NULL; arcs = ARC_LIST_rest(arcs) ){
      ARC* arc = ARC_LIST_first(arcs);
      if( ARC_kind(arc) == CG_DEP_REGIN ||
	  ARC_kind(arc) == CG_DEP_MEMIN ){
	OPR_uses(opr)++;
      }

      if( ARC_latency(arc) > OPR_latency(opr) )
	OPR_latency(opr) = ARC_latency(arc);
    }

    OPR_mem_index(opr) = OPR_mem_base(opr) = NULL;
    OPR_mem_sym(opr) = NULL;
    OPR_mem_ofst(opr) = 0;

    for( int i = 0; i < OP_results(op); i++ ){
      TN* result = OP_result( op, i );
      defop_by_reg[TN_register_class(result)][TN_register(result)] = op;
    }

    if( OP_memory( op ) ){
      const TOP top = OP_code(op);
      const int index_idx = TOP_Find_Operand_Use( top, OU_index );

      if( index_idx >= 0 ){
	OPR_mem_index(opr) = OP_opnd( op, index_idx );
      }

      const int base_idx = TOP_Find_Operand_Use( top, OU_base );
      if( base_idx >= 0 ){
	OPR_mem_base(opr) = OP_opnd( op, base_idx );
      }

      const int ofst_idx = TOP_Find_Operand_Use( top, OU_offset );
      TN* ofst_tn = OP_opnd( op, ofst_idx );
      int ofst = TN_value( ofst_tn );

      if( TN_is_symbol( ofst_tn ) ){
	ST* sym = TN_var( ofst_tn );
	ST* root_sym = NULL;
	INT64 root_offset = 0;
	Base_Symbol_And_Offset( sym, &root_sym, &root_offset);
	if( sym != root_sym ){
	  ofst += root_offset;
	}

	OPR_mem_sym(opr) = root_sym;
      }
      
      OPR_mem_ofst(opr) = ofst;

      mem_ops++;
    }
  }

  _true_cp = (int)ceil( mem_ops / 2 );
  mem_ops = (int)ceil( BB_length(bb) / mrt.issue_rate );
  _cp = _true_cp = std::max( _true_cp, mem_ops );

  // Compute the release time only thru true data dependence arcs.
  FOR_ALL_BB_OPs_FWD( bb, op ){
    OPR* opr = Get_OPR( op );

    for( ARC_LIST* arcs = OP_succs(op); arcs != NULL; arcs = ARC_LIST_rest(arcs) ){
      ARC* arc = ARC_LIST_first(arcs);

      if( ARC_kind(arc) != CG_DEP_REGIN &&
	  ARC_kind(arc) != CG_DEP_MEMIN )
	continue;

      OP* succ_op = ARC_succ(arc);
      OPR* succ_opr = Get_OPR( succ_op );

      int time = ARC_latency(arc) + OPR_release_time(opr);
      if( OPR_release_time(succ_opr) < time ){
	OPR_release_time(succ_opr) = time;
      }

      _true_cp = std::max( _true_cp, time );
    }
  }

  // Compute the release time.
  FOR_ALL_BB_OPs_FWD( bb, op ){
    OPR* opr = Get_OPR( op );

    OPR_issue_time( opr ) = OPR_release_time( opr );

    for( ARC_LIST* arcs = OP_succs(op); arcs != NULL; arcs = ARC_LIST_rest(arcs) ){
      ARC* arc = ARC_LIST_first(arcs);
      OP* succ_op = ARC_succ(arc);
      OPR* succ_opr = Get_OPR( succ_op );

      int time = ARC_latency(arc) + OPR_release_time(opr);
      if( OPR_release_time(succ_opr) < time ){
	OPR_release_time(succ_opr) = time;
      }

      _cp = std::max( _cp, time );
      OPR_num_succs(opr)++;
    }
  }

  // Compute the deadline.
  FOR_ALL_BB_OPs_REV( bb, op ){
    OPR* opr = Get_OPR( op );

    for( ARC_LIST* arcs = OP_preds(op); arcs != NULL; arcs = ARC_LIST_rest(arcs) ){
      ARC* arc = ARC_LIST_first(arcs);
      OP* pred_op = ARC_pred(arc);
      OPR* pred_opr = Get_OPR( pred_op );

      int time = OPR_deadline(opr) - ARC_latency(arc);
      if( OPR_deadline(pred_opr) > time ){
	OPR_deadline(pred_opr) = time;
	ASSERT( time >= OPR_release_time(pred_opr) );
      }

      OPR_num_preds(opr)++;
    }
  }

  if( trace ){
    FOR_ALL_BB_OPs_FWD( bb, op ){
      Print_OP_No_SrcLine(op);
      OPR* opr = Get_OPR( op );
      fprintf( TFile, "release_time:%d deadline:%d num_succs:%d num_preds:%d ",
	       OPR_release_time(opr), OPR_deadline(opr),
	       OPR_num_succs(opr), OPR_num_preds(opr) );
      fprintf( TFile, "uses:%d latency:%d ",
	       OPR_uses(opr), OPR_latency(opr) );
      fprintf( TFile, "\n" );
    }
  }
}


void KEY_SCH::Summary_BB()
{
  if( _cp > _true_cp + 1 ){
    const bool is_loop_body = ( ANNOT_Get(BB_annotations(bb), ANNOT_LOOPINFO) != NULL &&
				BB_xfer_op( bb ) != NULL );
    const int cycles = 1 + OP_scycle( BB_last_op(bb) );

    fprintf( TFile, "%c%s[%d] ops:%d cycles:%d cp:%d true_cp:%d\n",
	     BB_innermost(bb) ? '*' : ' ', Cur_PU_Name, BB_id(bb),
	     BB_length(bb), cycles, _cp, _true_cp );

    if( BB_innermost(bb) ){
      //Print_BB( bb );
    }
  }
}


void KEY_SCH::Tighten_Release_Time( OP* op )
{
  OPR* opr = Get_OPR( op );
  ASSERT( OPR_release_time(opr) <= OPR_deadline(opr) );

  for( ARC_LIST* arcs = OP_succs(op); arcs != NULL; arcs = ARC_LIST_rest(arcs) ){
    ARC* arc = ARC_LIST_first(arcs);
    OP* succ_op = ARC_succ(arc);
    OPR* succ_opr = Get_OPR( succ_op );

    int time = ARC_latency(arc) + OPR_release_time(opr);

    if( OPR_release_time(succ_opr) < time ){
      OPR_release_time(succ_opr) = time;
      Tighten_Release_Time( succ_op );
    }
  }
}


int KEY_SCH::Addr_Generation( OP* op )
{
  int time = -1;
  OPR* opr = Get_OPR( op );
  // Assume the address will be calculated as <N> ops are elapsed.
  const unsigned int N = 100;

  if( OPR_mem_index( opr ) != NULL ){
    TN* index = OPR_mem_index( opr );
    OP* pred_op = defop_by_reg[TN_register_class(index)][TN_register(index)];
    if( pred_op != NULL ){
      OPR* pred_opr = Get_OPR( pred_op );
      if( sched_order - OPR_sched_order( pred_opr ) < N )
	time = MAX( time, OPR_sched_order( pred_opr ) );
    }
  }

  if( OPR_mem_base( opr ) != NULL ){
    TN* base  = OPR_mem_base( opr );
    OP* pred_op = defop_by_reg[TN_register_class(base)][TN_register(base)];
    if( pred_op != NULL ){
      OPR* pred_opr = Get_OPR( pred_op );
      if( sched_order - OPR_sched_order( pred_opr ) < N )
	time = MAX( time, OPR_sched_order( pred_opr ) );
    }
  }

  return time;
}


OP* KEY_SCH::Winner( OP* op_a, OP* op_b )
{
  OPR* opr_a = Get_OPR( op_a );
  OPR* opr_b = Get_OPR( op_b );

#if 0
  if( mrt.Memory_Saturated( OPR_issue_time( opr_a ) ) ){
    if( OP_memory( op_a ) )
      return op_b;
    if( OP_memory( op_b ) )
      return op_a;
  }
#endif
  
  // Pick the one with the earliest completion time.
  if( OPR_issue_time( opr_a ) != OPR_issue_time( opr_b ) ){
    return ( OPR_issue_time( opr_a ) < OPR_issue_time( opr_b ) ? op_a : op_b );
  }

  // Pick the one to avoid address generation interlocking.
  if( OP_memory( op_a ) && OP_memory( op_b ) ){
    const int op_a_addr_cal = Addr_Generation( op_a );
    const int op_b_addr_cal = Addr_Generation( op_b );

    // Pick the one whose addrss is ready.
    if( op_a_addr_cal != op_b_addr_cal ){
      return ( op_a_addr_cal < op_b_addr_cal ? op_a : op_b );
    }

    // Pick the one whose address is close to <last_mem_op>
    if( last_mem_op != NULL ){
      const OPR* opr_last = Get_OPR( last_mem_op );
      const bool a_is_close =
	( ( OPR_mem_base( opr_a ) == OPR_mem_base( opr_last ) )   &&
	  ( OPR_mem_index( opr_a ) == OPR_mem_index( opr_last ) ) &&
	  ( OPR_mem_sym( opr_a ) == OPR_mem_sym( opr_last ) ) );
      const bool b_is_close =
	( ( OPR_mem_base( opr_b ) == OPR_mem_base( opr_last ) )   &&
	  ( OPR_mem_index( opr_b ) == OPR_mem_index( opr_last ) ) &&
	  ( OPR_mem_sym( opr_b ) == OPR_mem_sym( opr_last ) ) );

      if( a_is_close != b_is_close )
	return ( a_is_close ? op_a : op_b );
    }

    // Pick the one with lower address offset.
    if( ( OPR_mem_base( opr_a ) == OPR_mem_base( opr_b ) ) &&
	( OPR_mem_ofst( opr_a ) != OPR_mem_ofst( opr_b ) ) ){
      return ( OPR_mem_ofst( opr_a ) < OPR_mem_ofst( opr_b ) ? op_a : op_b );
    }

    // Pick the load first.
    if( OP_store( op_a ) || OP_store( op_b ) ){
      return ( OP_store( op_b ) ? op_a : op_b );
    }
  }

  // Pick the one with longer latency.
  if( OPR_latency( opr_a ) != OPR_latency( opr_b ) ){
    return ( OPR_latency( opr_a ) > OPR_latency( opr_b ) ? op_a : op_b );
  }

  // Pick the non-write first, since out-of-order writes are not allowed.
  if( OP_store( op_a ) && !OP_store( op_b ) )
    return op_b;

  if( !OP_store( op_a ) && OP_store( op_b ) )
    return op_a;
  
  // Pick the one has more uses.
  if( OPR_uses( opr_a ) != OPR_uses( opr_b ) ){
    return ( OPR_uses( opr_a ) > OPR_uses( opr_b ) ? op_a : op_b );
  }

  // Pick the one with the earliest deadline.
  if( OPR_deadline( opr_a ) != OPR_deadline( opr_b ) ){
    return ( OPR_deadline( opr_a ) < OPR_deadline( opr_b ) ) ? op_a : op_b;
  }

  // Pick the one whose predecessors are scheduled earlier.
  return OPR_pred_order( opr_a ) < OPR_pred_order( opr_b ) ? op_a : op_b;

  // Finally, stick with the original order.
  return OP_map_idx( op_a ) < OP_map_idx( op_b ) ? op_a : op_b;
}


OP* KEY_SCH::Select_Variable( int cycle )
{
  const int num = VECTOR_count( _ready_vector );
  OP* best = NULL;

  ASSERT( num > 0 );

  if( trace ){
    fprintf( TFile, "Ready list:\n" );
    for( int i = 0; i < num; i++ ){
      OP* op = (OP*)VECTOR_element( _ready_vector, i );
      Print_OP_No_SrcLine( op );
    }
  }

  for( int i = 0; i < num; i++ ){
    OP* op = (OP*)VECTOR_element( _ready_vector, i );
    OPR* opr = Get_OPR( op );

    if( OPR_release_time( opr ) == cycle ){
      best = ( best == NULL ) ? op : Winner( best, op );
    }
  }

  if( best == NULL ){
    /* If none of the ops in _ready_vector has its operands available,
       then pick one whatever. */
    best = (OP*)VECTOR_element( _ready_vector, 0 );

    for( int i = 1; i < num; i++ ){
      OP* op = (OP*)VECTOR_element( _ready_vector, i );
      OPR* opr = Get_OPR( op );
      best = Winner( best, op );
    }
  }

  OP_scycle( best ) = OPR_issue_time( Get_OPR(best) );

  if( trace ){
    fprintf( TFile, "Select: " );
    Print_OP_No_SrcLine( best );
  }

  return best;
}


void KEY_SCH::Schedule_BB()
{
  Build_OPR();
  Build_Ready_Vector();
  int cur_clock = 0;

  last_mem_op = NULL;

  while( VECTOR_count(_ready_vector) > 0 ){
    OP* op = Select_Variable( cur_clock );
    OPR* opr = Get_OPR( op );

    //if( OP_imul(op) && mrt.Decoded_Ops(cur_clock) > 0 )      cur_clock++;

    if( trace ){
      fprintf( TFile, "Cycle[%d] : issue_time %d",
	       cur_clock, OPR_issue_time(opr) );
      Print_OP_No_SrcLine( op );
    }

    if( OP_memory( op ) ){
      last_mem_op = op;
    }
    VECTOR_Delete_Element( _ready_vector, op );
    Set_OPR_is_scheduled( opr );
    OPR_sched_order(opr) = sched_order++;
    VECTOR_Add_Element( _sched_vector, op );

    ASSERT( OPR_release_time(opr) <= OPR_issue_time(opr) );

    // Update ready vector.
    for( ARC_LIST* arcs = OP_succs(op); arcs != NULL; arcs = ARC_LIST_rest(arcs) ){
      ARC* arc = ARC_LIST_first(arcs);
      OP* succ_op = ARC_succ(arc);
      OPR* succ_opr = Get_OPR( succ_op );
      OPR_num_preds( succ_opr )--;
      if( OPR_num_preds( succ_opr ) == 0 ){
	VECTOR_Add_Element( _ready_vector, succ_op );
      }

      OPR_pred_order( succ_opr ) = OPR_sched_order( opr );
      ASSERT( cur_clock + ARC_latency(arc) <= OPR_deadline( succ_opr ) );

      if( ARC_kind(arc) == CG_DEP_REGIN &&
	  OPR_issue_time(opr) + ARC_latency(arc) > OPR_release_time(succ_opr) ){
	OPR_release_time(succ_opr) = OPR_issue_time(opr) + ARC_latency(arc);
	Tighten_Release_Time( succ_op );
      }
    }

    mrt.Reserve_Resources( op, cur_clock );

    if( mrt.Decoder_is_Saturated( cur_clock ) ){
      cur_clock++;

      // Tighten scheduling ranges based on the update release.
      for( int i = VECTOR_count( _ready_vector ) - 1; i >= 0; i-- ){
	OP* ready_op = (OP*)VECTOR_element( _ready_vector, i );
	OPR* ready_opr = Get_OPR( ready_op );

	if( OPR_release_time( ready_opr ) < cur_clock ){
	  OPR_release_time( ready_opr ) = cur_clock;
	  Tighten_Release_Time( ready_op );
	}
      }
    }

    /* Given the update <cur_clock> or resources usage, update the issue time
       for all the ready ops.
    */

    for( int i = VECTOR_count( _ready_vector ) - 1; i >= 0; i-- ){
      OP* ready_op = (OP*)VECTOR_element( _ready_vector, i );
      mrt.Compute_Issue_Time( ready_op, cur_clock );
    }
  }
}


KEY_SCH::KEY_SCH( BB* bb, MEM_POOL* pool, BOOL trace )
  : bb( bb ), mem_pool( pool ), trace( trace )
{
  if( CG_skip_local_sched ){
    BBs_Processed++;
    if( BBs_Processed < CG_local_skip_before ||
	BBs_Processed > CG_local_skip_after  ||
	BBs_Processed == CG_local_skip_equal ){
      fprintf( TFile, "[%d] BB:%d processed in KEY_SCH\n", 
	       BBs_Processed, BB_id(bb) );
      return;
    }
  }

#if 0
  if( !( strcmp( Cur_PU_Name, "resid_" ) == 0 &&
	 BB_id( bb ) == 8 ) ){
    return;
  }
#endif

  if( BB_length(bb) > 2 ){
    CG_DEP_Compute_Graph( bb, 
			  INCLUDE_ASSIGNED_REG_DEPS,
			  NON_CYCLIC,
			  INCLUDE_MEMREAD_ARCS,
			  INCLUDE_MEMIN_ARCS,
			  INCLUDE_CONTROL_ARCS,
			  NULL );

    if( trace ){
      CG_DEP_Trace_Graph( bb );
    }

    Init();
    Schedule_BB();
    Reorder_BB();
    Summary_BB();

    CG_DEP_Delete_Graph( bb );
  }

  Set_BB_scheduled( bb );
  if( Assembly && BB_length(bb) > 0 )
    Add_Scheduling_Note( bb, NULL );
}


void CG_Sched( MEM_POOL* pool, BOOL trace )
{
  return;

  // compute live-in sets for registers.
  REG_LIVE_Analyze_Region();

  for( BB* bb = REGION_First_BB; bb != NULL; bb = BB_next(bb) ){
    RID* rid = BB_rid(bb);
    if( rid != NULL &&
	RID_level(rid) >= RL_CGSCHED )
      continue;

    KEY_SCH sched( bb, pool, trace );
  }

  // Finish with reg_live after cflow so that it can benefit from the info.
  REG_LIVE_Finish ();
}
