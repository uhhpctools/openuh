extern "C" {
#include "omp_rtl.h"
#include "omp_sys.h"
int __omp_collector_api(void *arg);
}
#include "omp_collector_api.h"
#include <queue>
using namespace std;
static int init_lock = 0;
int collector_initialized=0;
static int paused_lock=0;
int collector_paused=0;
static int event_lock=0;
int process_top_request(omp_collector_message &req);
int register_event(omp_collector_message &req);
int unregister_event(omp_collector_message &req);
int return_state(omp_collector_message &req);
int return_current_prid(omp_collector_message &req);
int return_parent_prid(omp_collector_message &req);

extern omp_v_thread_t * __omp_level_1_team;

int __omp_collector_api(void *arg)
{
    
    if(arg!=NULL)
    {
        queue<omp_collector_message> pending_requests;
        char *traverse = (char *) arg;
 
        while((int)(*traverse)!=0)
        {
            omp_collector_message req;
            req.sz = (int)(*traverse); // todo: add check for consistency    
	    traverse+=sizeof(int);
            req.r = (OMP_COLLECTORAPI_REQUEST)(*traverse);  // todo: add check for a valid request
            traverse+=sizeof(int);      
            req.ec= (OMP_COLLECTORAPI_EC *) traverse;  // copy address for response of error code
            traverse+=sizeof(int);    
            req.rsz = (int *)(traverse);
            traverse+=sizeof(int);
            req.mem = traverse;
            traverse+=req.sz-(4*sizeof(int));
            pending_requests.push(req);
        } 

       while(!pending_requests.empty()) {
                 omp_collector_message pop_req= pending_requests.front();
                 pending_requests.pop();
                 process_top_request(pop_req);  
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
   __ompc_spin_lock(&event_lock);
   __omp_level_1_team_manager.callbacks[i]= NULL;
   __ompc_spin_unlock(&event_lock);
  } // note check callback boundaries.
   __ompc_spin_lock(&init_lock);
   collector_initialized = 1;
   __ompc_spin_unlock(&init_lock);
  *(req.ec) = OMP_ERRCODE_OK;
  }
  else {
 
  *(req.ec) = OMP_ERRCODE_SEQUENCE_ERR;
  }
   
   *(req.rsz) =0;
}


void __ompc_req_stop(omp_collector_message &req)
{
  int i;
   static int init_lock = 0;
  
  if(collector_initialized) {
    __ompc_spin_lock(&init_lock);
  collector_initialized = 0;
    __ompc_spin_unlock(&init_lock);
   __ompc_spin_lock(&event_lock);
  for (i=0; i< OMP_EVENT_THR_END_ATWT+1; i++) {
   __omp_level_1_team_manager.callbacks[i]= NULL;
  } 
  __ompc_spin_unlock(&event_lock);
  // note check callback boundaries.
  
  *(req.ec) = OMP_ERRCODE_OK;
  }
  else {
 
  *(req.ec) = OMP_ERRCODE_SEQUENCE_ERR;
  } 
   *(req.rsz) =0;
}

int __ompc_req_pause(omp_collector_message &req)
{
     if(collector_initialized) {
                 
		  __ompc_spin_lock(&paused_lock); 
                 collector_paused = 1;
                 __ompc_spin_unlock(&paused_lock);     
                 *req.ec = OMP_ERRCODE_OK;
	    }  
            else {
	      *req.ec = OMP_ERRCODE_SEQUENCE_ERR;
	    } 
     *req.rsz = 0;
     return 1;
}


int __ompc_req_resume(omp_collector_message &req)
{
     if(collector_initialized) { 
                __ompc_spin_lock(&paused_lock); 
                 collector_paused = 0;
	        __ompc_spin_unlock(&paused_lock);
                 *req.ec = OMP_ERRCODE_OK;
	    }  
            else {
	      *req.ec = OMP_ERRCODE_SEQUENCE_ERR;
	    } 
     *req.rsz = 0;
     return 1;
}

int process_top_request(omp_collector_message &req)
{
  
     
     
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
	  __ompc_req_stop(req);
	  break;

          case OMP_REQ_PAUSE:
	  __ompc_req_pause(req); 
	  break;

          case OMP_REQ_RESUME:
	  __ompc_req_resume(req);
	  break;

          default:
	    *(req.ec) = OMP_ERRCODE_UNKNOWN;
            *(req.rsz) = 0;   
	  break;
      }
      return 1;
   
}

int event_is_valid(OMP_COLLECTORAPI_EVENT e)
{
  /* this needs to be improved with something more portable when we extend the events in the runtime */
  if (e>=OMP_EVENT_FORK && e<=OMP_EVENT_THR_END_ATWT)
    return 1; 
  else
    return 0;
}

int event_is_supported(OMP_COLLECTORAPI_EVENT e)
{
  int event_supported=1;
  switch (e) {
     case OMP_EVENT_THR_BEGIN_ATWT:
     case OMP_EVENT_THR_END_ATWT:
       event_supported=0;
     break;

     default:
     break;
  }

  return event_supported;
}
int register_event(omp_collector_message &req)
{    
  if(collector_initialized) {
   OMP_COLLECTORAPI_EVENT  *event = (OMP_COLLECTORAPI_EVENT *)req.mem;
   unsigned long *temp_mem = (unsigned long *)(req.mem + sizeof(OMP_COLLECTORAPI_EVENT));
   if(event_is_valid(*event)) {
      __ompc_spin_lock(&event_lock);
      __omp_level_1_team_manager.callbacks[*event] = (void (*)(OMP_COLLECTORAPI_EVENT)) (*temp_mem);
      __ompc_spin_unlock(&event_lock); 
      if(event_is_supported(*event)) *req.ec=OMP_ERRCODE_OK;
      else *req.ec=OMP_ERRCODE_UNSUPPORTED;
   }
   else {
       *req.ec=OMP_ERRCODE_UNKNOWN;
     }
     
  } 
  else {
    *req.ec=OMP_ERRCODE_SEQUENCE_ERR;
  }
  
  *req.rsz = 0;

  return 1;
}

int unregister_event(omp_collector_message &req)
{

  if(collector_initialized) {
  OMP_COLLECTORAPI_EVENT  *event = (OMP_COLLECTORAPI_EVENT *)req.mem;
   if(event_is_valid(*event)) {
      __ompc_spin_lock(&event_lock);
    __omp_level_1_team_manager.callbacks[*event] = NULL;
    __ompc_spin_unlock(&event_lock);

    if(event_is_supported(*event)) *req.ec=OMP_ERRCODE_OK;
    else *req.ec=OMP_ERRCODE_UNSUPPORTED;
   }
   else {

     *req.ec=OMP_ERRCODE_UNKNOWN;
   }
  }
  else {
    *req.ec=OMP_ERRCODE_SEQUENCE_ERR;
  }
  
  return 1;
}

/* needs to be thread safe */
int return_state_id(omp_collector_message &req,long id)
{
  bool possible_mem_prob = (req.sz - 4*sizeof(int)) < (sizeof(OMP_COLLECTOR_API_THR_STATE)+sizeof(unsigned long)); 
  if(!possible_mem_prob){ 
         *((unsigned long *)(req.mem+sizeof(OMP_COLLECTOR_API_THR_STATE)))=id; 
         *(req.rsz) = sizeof(OMP_COLLECTOR_API_THR_STATE)+sizeof(unsigned long);
         *(req.ec) = OMP_ERRCODE_OK; 
  }
   else {
        *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
        *(req.rsz)=0;
        return 0;
   }
  return 1;
}
int return_state(omp_collector_message &req)
{
  if(!collector_initialized)
   {
     *(req.rsz)=0;
     *(req.ec)=OMP_ERRCODE_SEQUENCE_ERR;
     return 0;
   } 

  if((req.sz - 4*sizeof(int)) < sizeof(OMP_COLLECTOR_API_THR_STATE)) {
    *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
     return 0;
  }
  else {
   
   omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid); 
   *((OMP_COLLECTOR_API_THR_STATE *) req.mem) = (OMP_COLLECTOR_API_THR_STATE) p_vthread->state;
   switch(p_vthread->state) {
  
   case THR_IBAR_STATE:
     return return_state_id(req,p_vthread->thr_ibar_state_id);
  
   case THR_EBAR_STATE:    	      
   return return_state_id(req,p_vthread->thr_ebar_state_id);    
 
	  	
   case THR_LKWT_STATE:
     return return_state_id(req,p_vthread->thr_lkwt_state_id);     
	       
   case THR_CTWT_STATE:
     return return_state_id(req,p_vthread->thr_ctwt_state_id);
	       
   case THR_ODWT_STATE:
   return return_state_id(req,p_vthread->thr_odwt_state_id);
       
   case THR_ATWT_STATE:
   return return_state_id(req,p_vthread->thr_atwt_state_id);
 
   default:
     *(req.rsz)=sizeof(OMP_COLLECTOR_API_THR_STATE);
     *(req.ec) = OMP_ERRCODE_OK;
   return 1;
   break; 
 
   }
  }

  return 1;
}

int return_current_prid(omp_collector_message &req)
{ 
     if(!collector_initialized) {
     *(req.rsz)=0;
     *(req.ec)=OMP_ERRCODE_SEQUENCE_ERR;
     return 0;
   } 

    if((req.sz - 4*sizeof(int)) < sizeof(unsigned long)) {
    *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
    *(req.rsz)=0;
     return 0;
    }
    else {
      if(__ompc_in_parallel() || __omp_root_v_thread.state!=THR_SERIAL_STATE ) {
	*((unsigned long *)req.mem) = current_region_id; }
	else *((unsigned long *)req.mem) = 0;
        *(req.rsz)=sizeof(unsigned long);
    }
    return 1;
}       

int return_parent_prid(omp_collector_message &req)
{
      if(!collector_initialized) {
     *(req.rsz)=0;
     *(req.ec)=OMP_ERRCODE_SEQUENCE_ERR;
     return 0;
   } 


     if((req.sz - 4*sizeof(int)) < sizeof(unsigned long)) {
        *(req.rsz)=0;
        *(req.ec) = OMP_ERRCODE_MEM_TOO_SMALL;
        return 0;
    }
    else {
      if(__ompc_in_parallel() || __omp_root_v_thread.state!=THR_SERIAL_STATE) {
	*((unsigned long *)req.mem) = current_region_id; }
       else *((unsigned long *)req.mem) = 0;   
      *(req.rsz)=sizeof(unsigned long);
    }
     return 1;
}
       
       
       
