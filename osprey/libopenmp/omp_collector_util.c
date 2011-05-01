#include "omp_rtl.h"
#include "omp_lock.h"
#include "omp_collector_api.h"
#include <assert.h>

char OMP_EVENT_NAME[22][50]= {
  "OMP_EVENT_FORK",
  "OMP_EVENT_JOIN",
  "OMP_EVENT_THR_BEGIN_IDLE",
  "OMP_EVENT_THR_END_IDLE",
  "OMP_EVENT_THR_BEGIN_IBAR",
  "OMP_EVENT_THR_END_IBAR",
  "OMP_EVENT_THR_BEGIN_EBAR",
  "OMP_EVENT_THR_END_EBAR",
  "OMP_EVENT_THR_BEGIN_LKWT",
  "OMP_EVENT_THR_END_LKWT",
  "OMP_EVENT_THR_BEGIN_CTWT",
  "OMP_EVENT_THR_END_CTWT",
  "OMP_EVENT_THR_BEGIN_ODWT",
  "OMP_EVENT_THR_END_ODWT",
  "OMP_EVENT_THR_BEGIN_MASTER",
  "OMP_EVENT_THR_END_MASTER",
  "OMP_EVENT_THR_BEGIN_SINGLE",
  "OMP_EVENT_THR_END_SINGLE",
  "OMP_EVENT_THR_BEGIN_ORDERED",
  "OMP_EVENT_THR_END_ORDERED",
  "OMP_EVENT_THR_BEGIN_ATWT",
  "OMP_EVENT_THR_END_ATWT" };


char OMP_STATE_NAME[11][50]= {
  "THR_OVHD_STATE",          /* Overhead */
  "THR_WORK_STATE",          /* Useful work, excluding reduction, master, single, critical */
  "THR_IBAR_STATE",          /* In an implicit barrier */
  "THR_EBAR_STATE",          /* In an explicit barrier */
  "THR_IDLE_STATE",          /* Slave waiting for work */
  "THR_SERIAL_STATE",        /* thread not in any OMP parallel region (initial thread only) */
  "THR_REDUC_STATE",         /* Reduction */
  "THR_LKWT_STATE",          /* Waiting for lock */
  "THR_CTWT_STATE",          /* Waiting to enter critical region */
  "THR_ODWT_STATE",          /* Waiting to execute an ordered region */
  "THR_ATWT_STATE"};         /* Waiting to enter an atomic region */

int __omp_collector_api(void *arg);

static ompc_spinlock_t init_lock;
int collector_initialized=0;
static ompc_spinlock_t paused_lock;
int collector_paused=0;
static ompc_spinlock_t event_lock;
int process_top_request(omp_collector_message * req);
int register_event(omp_collector_message * req);
int unregister_event(omp_collector_message *req);
int return_state(omp_collector_message *req);
int return_current_prid(omp_collector_message *req);
int return_parent_prid(omp_collector_message *req);

extern omp_v_thread_t* __omp_level_1_team;

struct message_queue_node_s {
  omp_collector_message message;
  struct message_queue_node_s* next;
};
typedef struct message_queue_node_s message_queue_node_t;

typedef struct {
  message_queue_node_t* head;
  message_queue_node_t* tail;
} message_queue_t;

static inline void message_queue_init(message_queue_t* queue)
{
  queue->head = queue->tail = NULL;
}

/*
static inline void message_queue_destroy(message_queue_t* queue)
{
  message_queue_node_t* head = queue->head;
  message_queue_node_t* last_head;
  while (head) {
    last_head = head;
    head = head->next;
    free(last_head);
  }
}
*/

static inline omp_collector_message* message_queue_push(message_queue_t* queue)
{
  message_queue_node_t* new_node = (message_queue_node_t*) malloc(sizeof(message_queue_node_t));
  new_node->next = NULL;
  if (queue->tail) {
    queue->tail->next = new_node;
  } else {
    queue->head = new_node;
  }
  queue->tail = new_node;
  return &(new_node->message);
}

static inline void message_queue_pop(message_queue_t* queue, omp_collector_message* msg)
{
  message_queue_node_t* cur_node;
  assert(queue->head);
  cur_node = queue->head;
  *msg = cur_node->message;
  queue->head = cur_node->next;
  if (!queue->head) {
    queue->tail = NULL;
  }
  free(cur_node);
}

static inline int message_queue_empty(message_queue_t* queue)
{
  return queue->head == NULL;
}


int __omp_collector_api(void *arg)
{
    
  if(arg!=NULL) {
    message_queue_t pending_requests;
    char *traverse = (char *) arg;

    message_queue_init(&pending_requests);
 
    while((int)(*traverse)!=0) {
      omp_collector_message* req = message_queue_push(&pending_requests);
      req->sz = (int)(*traverse); // todo: add check for consistency    
      traverse+=sizeof(int);
      req->r = (OMP_COLLECTORAPI_REQUEST)(*traverse);  // todo: add check for a valid request
      traverse+=sizeof(int);      
      req->ec= (OMP_COLLECTORAPI_EC *) traverse;  // copy address for response of error code
      traverse+=sizeof(int);    
      req->rsz = (int *)(traverse);
      traverse+=sizeof(int);
      req->mem = traverse;
      traverse+=req->sz-(4*sizeof(int));
    } 

    while(!message_queue_empty(&pending_requests)) {
      omp_collector_message pop_req;
      message_queue_pop(&pending_requests, &pop_req);
      process_top_request(&pop_req);  
    }
    return 0;
  }
  return -1;
}

void __ompc_req_start(omp_collector_message *req)
{
  int i;
  
  if(!collector_initialized) {
    for (i=0; i< OMP_EVENT_THR_END_ATWT+1; i++) {
      __ompc_lock_spinlock(&event_lock);
      __omp_level_1_team_manager.callbacks[i]= NULL;
      __ompc_unlock_spinlock(&event_lock);
    } // note check callback boundaries.
    __ompc_lock_spinlock(&init_lock);
    collector_initialized = 1;
    __ompc_unlock_spinlock(&init_lock);
    *(req->ec) = OMP_ERRCODE_OK;
  } else {
    *(req->ec) = OMP_ERRCODE_SEQUENCE_ERR;
  }
   
  *(req->rsz) =0;
}


void __ompc_req_stop(omp_collector_message *req)
{
  int i;

  if(collector_initialized) {
    __ompc_lock_spinlock(&init_lock);
    collector_initialized = 0;
    __ompc_unlock_spinlock(&init_lock);
    __ompc_lock_spinlock(&event_lock);
    for (i=0; i< OMP_EVENT_THR_END_ATWT+1; i++) {
      __omp_level_1_team_manager.callbacks[i]= NULL;
    } 
    __ompc_unlock_spinlock(&event_lock);
    // note check callback boundaries.
  
    *(req->ec) = OMP_ERRCODE_OK;
  } else {
    *(req->ec) = OMP_ERRCODE_SEQUENCE_ERR;
  } 
  *(req->rsz) =0;
}

int __ompc_req_pause(omp_collector_message *req)
{
  if(collector_initialized) {
                 
    __ompc_lock_spinlock(&paused_lock); 
    collector_paused = 1;
    __ompc_unlock_spinlock(&paused_lock);     
    *(req->ec) = OMP_ERRCODE_OK;
  } else {
    *(req->ec) = OMP_ERRCODE_SEQUENCE_ERR;
  } 
  *(req->rsz) = 0;
  return 1;
}


int __ompc_req_resume(omp_collector_message *req)
{
  if(collector_initialized) { 
    __ompc_lock_spinlock(&paused_lock); 
    collector_paused = 0;
    __ompc_unlock_spinlock(&paused_lock);
    *(req->ec) = OMP_ERRCODE_OK;
  } else {
    *(req->ec) = OMP_ERRCODE_SEQUENCE_ERR;
  } 
  *(req->rsz) = 0;
  return 1;
}

int process_top_request(omp_collector_message *req)
{
  switch(req->r) {
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
    *(req->ec) = OMP_ERRCODE_UNKNOWN;
    *(req->rsz) = 0;   
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
int register_event(omp_collector_message *req)
{    
  if(collector_initialized) {
    OMP_COLLECTORAPI_EVENT  *event = (OMP_COLLECTORAPI_EVENT *)req->mem;
    unsigned long *temp_mem = (unsigned long *)(req->mem + sizeof(OMP_COLLECTORAPI_EVENT));
    if(event_is_valid(*event)) {
      __ompc_lock_spinlock(&event_lock);
      __omp_level_1_team_manager.callbacks[*event] = (void (*)(OMP_COLLECTORAPI_EVENT)) (*temp_mem);
      __ompc_unlock_spinlock(&event_lock); 
      if(event_is_supported(*event)) *(req->ec)=OMP_ERRCODE_OK;
      else *(req->ec)=OMP_ERRCODE_UNSUPPORTED;
    } else {
      *(req->ec)=OMP_ERRCODE_UNKNOWN;
    }
  } else {
    *(req->ec)=OMP_ERRCODE_SEQUENCE_ERR;
  }
  
  *(req->rsz) = 0;

  return 1;
}

int unregister_event(omp_collector_message *req)
{

  if(collector_initialized) {
    OMP_COLLECTORAPI_EVENT  *event = (OMP_COLLECTORAPI_EVENT *)req->mem;
    if(event_is_valid(*event)) {
      __ompc_lock_spinlock(&event_lock);
      __omp_level_1_team_manager.callbacks[*event] = NULL;
      __ompc_unlock_spinlock(&event_lock);

      if(event_is_supported(*event))
        *(req->ec)=OMP_ERRCODE_OK;
      else
        *(req->ec)=OMP_ERRCODE_UNSUPPORTED;
    } else {
      *(req->ec)=OMP_ERRCODE_UNKNOWN;
    }
  } else {
    *(req->ec)=OMP_ERRCODE_SEQUENCE_ERR;
  }
  return 1;
}

/* needs to be thread safe */
int return_state_id(omp_collector_message *req,long id)
{
  int possible_mem_prob = (req->sz - 4*sizeof(int)) < (sizeof(OMP_COLLECTOR_API_THR_STATE)+sizeof(unsigned long)); 
  if(!possible_mem_prob){ 
    *((unsigned long *)(req->mem+sizeof(OMP_COLLECTOR_API_THR_STATE)))=id; 
    *(req->rsz) = sizeof(OMP_COLLECTOR_API_THR_STATE)+sizeof(unsigned long);
    *(req->ec) = OMP_ERRCODE_OK; 
  } else {
    *(req->ec) = OMP_ERRCODE_MEM_TOO_SMALL;
    *(req->rsz)=0;
    return 0;
  }
  return 1;
}
int return_state(omp_collector_message *req)
{
  if(!collector_initialized) {
    *(req->rsz)=0;
    *(req->ec)=OMP_ERRCODE_SEQUENCE_ERR;
    return 0;
  } 

  if((req->sz - 4*sizeof(int)) < sizeof(OMP_COLLECTOR_API_THR_STATE)) {
    *(req->ec) = OMP_ERRCODE_MEM_TOO_SMALL;
    return 0;
  } else {
   
    omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid); 
    *((OMP_COLLECTOR_API_THR_STATE *) req->mem) = (OMP_COLLECTOR_API_THR_STATE) p_vthread->state;
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
      *(req->rsz)=sizeof(OMP_COLLECTOR_API_THR_STATE);
      *(req->ec) = OMP_ERRCODE_OK;
      return 1;
      break; 
 
    }
  }

  return 1;
}

int return_current_prid(omp_collector_message *req)
{ 
  if(!collector_initialized) {
    *(req->rsz)=0;
    *(req->ec)=OMP_ERRCODE_SEQUENCE_ERR;
    return 0;
  } 

  if((req->sz - 4*sizeof(int)) < sizeof(unsigned long)) {
    *(req->ec) = OMP_ERRCODE_MEM_TOO_SMALL;
    *(req->rsz)=0;
    return 0;
  } else {
    if(__ompc_in_parallel() || __omp_root_v_thread.state!=THR_SERIAL_STATE ) {
      *((unsigned long *)req->mem) = current_region_id; }
    else *((unsigned long *)req->mem) = 0;
    *(req->rsz)=sizeof(unsigned long);
  }
  return 1;
}       

int return_parent_prid(omp_collector_message *req)
{
  if(!collector_initialized) {
    *(req->rsz)=0;
    *(req->ec)=OMP_ERRCODE_SEQUENCE_ERR;
    return 0;
  } 


  if((req->sz - 4*sizeof(int)) < sizeof(unsigned long)) {
    *(req->rsz)=0;
    *(req->ec) = OMP_ERRCODE_MEM_TOO_SMALL;
    return 0;
  } else {
    if(__ompc_in_parallel() || __omp_root_v_thread.state!=THR_SERIAL_STATE) {
      *((unsigned long *)req->mem) = current_region_id; }
    else *((unsigned long *)req->mem) = 0;   
    *(req->rsz)=sizeof(unsigned long);
  }
  return 1;
}
       
void __omp_collector_init() {
  __ompc_init_spinlock(&init_lock);
  __ompc_init_spinlock(&paused_lock);
  __ompc_init_spinlock(&event_lock);
}
