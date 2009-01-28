#include "omp_rtl.h"
#include "omp_collector_api.h"

#include <queue>
using namespace std;
int collector_intialized=0;
int collector_paused=0;
queue<omp_collector_message> pending_requests;
int process_top_request(void);
int register_event(omp_collector_message &req);
int unregister_event(omp_collector_message &req);
extern omp_v_thread_t * __omp_level_1_team;
int omp_collector_api(void *arg)
{
    if(arg!=NULL)
    {
        char *traverse = (char *) arg;
        while((int)(*traverse)=!0)
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



int check_consistency(omp_collector_message &req)
{
     //req



}
int add_pending_request(omp_collector_message &req)
{
    pending_requests.push(req);
}

int process_top_request(void)
{
  
      omp_collector_message req= pending_requests.front();
      pending_requests.pop();
     
      switch(req.r)
      {
          case OMP_REQ_START:
	  collector_intialized =1;   
	  break;

          case OMP_REQ_REGISTER:
          register_event(req);
          break;

          case OMP_REQ_UNREGISTER:
          register_event(req);
          break; 
          
          case OMP_REQ_STATE:
          break; 

          case OMP_REQ_CURRENT_PRID:
	  break;

          case OMP_REQ_PARENT_PRID:
	  break;

          case OMP_REQ_STOP:
          collector_intialized =0;
	  break;

          case OMP_REQ_PAUSE:
	  break;

          case OMP_REQ_RESUME:
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
    __omp_level_1_team_manager.callbacks[*event] = NULL;

  return 1;
}



int remove_pending_request(omp_collector_message &req)
{
      

}

