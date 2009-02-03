extern "C" {
#include "omp_rtl.h"
}
#include "omp_collector_api.h"
#include <queue>
using namespace std;
int collector_initialized=0;
int collector_paused=0;
queue<omp_collector_message> pending_requests;
int process_top_request(void);
int register_event(omp_collector_message &req);
int unregister_event(omp_collector_message &req);
int return_state(omp_collector_message &req);
int return_current_prid(omp_collector_message &req);
int return_parent_prid(omp_collector_message &req);

extern omp_v_thread_t * __omp_level_1_team;

int omp_collector_api(void *arg)
{
    if(arg!=NULL)
    {
        char *traverse = (char *) arg;
 
        while((int)(*traverse)!=0)
        {
            omp_collector_message req;
            req.sz = (int)(*traverse); // todo: add check for consistency    
	    traverse+=sizeof(int);
            req.r = (int)(*traverse);  // todo: add check for a valid request
            traverse+=sizeof(int);      
            req.ec= (int *) traverse;  // copy address for response of error code
            traverse+=sizeof(int);    
            req.rsz = (int)(*traverse);
            traverse+=sizeof(int);
            req.mem = traverse;
            traverse+=req.rsz;
            pending_requests.push(req);
        } 

       while(pending_requests.empty()) {
              process_top_request();  
                 }
	return 0;
    }
    return -1;
}

void __ompc_req_start(omp_collector_message &req)
{
  int i;
  if(!collector_initialized) {
  for (i=0; i< OMP_EVENT_THR_END_ATWT+1; i++) {
   __omp_level_1_team_manager.callbacks[i]= NULL;
  } // note check callback boundaries.
  
   collector_initialized = 1;
  *(req.ec) = OMP_ERRCODE_OK;
  }
  else {
 
  *(req.ec) = OMP_ERRCODE_SEQUENCE_ERR;
  }
}

int check_consistency(omp_collector_message &req)
{
     //req

}

int process_top_request(void)
{
  
      omp_collector_message req= pending_requests.front();
      pending_requests.pop();
     
      switch(req.r)
      {
          case OMP_REQ_START:
	  __ompc_req_start(req);   
	  break;

          case OMP_REQ_REGISTER:
          register_event(req);
          break;

          case OMP_REQ_UNREGISTER:
          unregister_event(req);
          break; 
          
          case OMP_REQ_STATE:
	  return_state(req);
          break; 

          case OMP_REQ_CURRENT_PRID:
	  return_current_prid(req);
	  break;

          case OMP_REQ_PARENT_PRID:
	  return_parent_prid(req);
	  break;

          case OMP_REQ_STOP:
          collector_initialized =0;
	  break;

          case OMP_REQ_PAUSE:
	  collector_paused = 1;
	  break;

          case OMP_REQ_RESUME:
	  collector_paused = 0;
	  break;

          default:
	    *(req.ec) = OMP_ERRCODE_UNKNOWN;   
	  break;
      }
      return 1;
   
}
int register_event(omp_collector_message &req)
{
    
   OMP_COLLECTORAPI_EVENT  *event = (OMP_COLLECTORAPI_EVENT *)req.mem;
   void (*func)(OMP_COLLECTORAPI_EVENT e) = (void (*)(OMP_COLLECTORAPI_EVENT))(req.mem+sizeof(OMP_COLLECTORAPI_EVENT));
   __omp_level_1_team_manager.callbacks[*event] = func;

  return 1;
}

int unregister_event(omp_collector_message &req)
{
  OMP_COLLECTORAPI_EVENT  *event = (OMP_COLLECTORAPI_EVENT *)req.mem;
  if(__omp_level_1_team_manager.callbacks[*event]) {

    __omp_level_1_team_manager.callbacks[*event] = NULL;
  }
  return 1;
}

/* needs to be thread safe */
int return_state_id(omp_collector_message &req,long id)
{
  bool possible_mem_prob = req.rsz < (sizeof(OMP_COLLECTOR_API_THR_STATE)+sizeof(long)); 
   if(!possible_mem_prob) *(req.mem+sizeof(OMP_COLLECTOR_API_THR_STATE))=id;
   else {
        *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
        return 0;
   }
  return 1;
}
int return_state(omp_collector_message &req)
{

  if(req.rsz < sizeof(OMP_COLLECTOR_API_THR_STATE)) {
    *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
     return 0;
  }
  else {
   
   omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid); 
   *(req.mem) = p_vthread->state;
   switch(p_vthread->state) {
  
   case THR_IBAR_STATE:
     return return_state_id(req,thr_ibar_state_id);
  
   case THR_EBAR_STATE:    	      
   return return_state_id(req,thr_ebar_state_id);    
 
	  	
   case THR_LKWT_STATE:
     return return_state_id(req,thr_lkwt_state_id);     
	       
   case THR_CTWT_STATE:
     return return_state_id(req,thr_ctwt_state_id);
	       
   case THR_ODWT_STATE:
   return return_state_id(req,thr_odwt_state_id);
       
   case THR_ATWT_STATE:
   return return_state_id(req,thr_atwt_state_id);
 
   default:
   return 1;
   break; 
 
   }
  }

  return 1;
}

int return_current_prid(omp_collector_message &req)
{ 
    if(req.rsz < sizeof(long)) {
    *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
     return 0;
    }
    else {
      if(__ompc_in_parallel()) {
	*(req.mem) = current_region_id; }
	else *(req.mem) = 0;

    }
    return 1;
}       

int return_parent_prid(omp_collector_message &req)
{
     if(req.rsz < sizeof(long)) {
        *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
        return 0;
    }
    else {
      if(__ompc_in_parallel()) {
	*(req.mem) = current_region_id; }
	else *(req.mem) = 0;   
    }
     return 1;
}
       
       
       
