#include <stdlib.h>
#include <stdio.h>
#include "omp_collector_api.h"
#include "omp_collector_validation.h"
#include "omp_collector_util.h"

const int OMP_COLLECTORAPI_HEADERSIZE=4*sizeof(int);

/* This routine is to test the OMP_REQ_START request to the collector */

void dummyfunc(OMP_COLLECTORAPI_EVENT event)
{
  printf("\n** EVENT:%s **",OMP_EVENT_NAME[event-1]);
}

void fill_header(void *message, int sz, OMP_COLLECTORAPI_REQUEST rq, OMP_COLLECTORAPI_EC ec, int rsz, int append_zero)
{
    int *psz = (int *) message; 
   *psz = sz;
   
   OMP_COLLECTORAPI_REQUEST *rnum = (OMP_COLLECTORAPI_REQUEST *) (message+sizeof(int));
   *rnum = rq;
   
   OMP_COLLECTORAPI_EC *pec = (OMP_COLLECTORAPI_EC *)(message+(sizeof(int)*2));
   *pec = ec;

   int *prsz = (int *) (message+ sizeof(int)*3);
   *prsz = rsz;

   if(append_zero) {
    psz = (int *)(message+(sizeof(int)*4)+rsz);
   *psz =0; 
   }   
  
}

void fill_register(void *message, OMP_COLLECTORAPI_EVENT event, int append_func, void (*func)(OMP_COLLECTORAPI_EVENT e), int append_zero)
{

  OMP_COLLECTORAPI_EVENT *pevent = (OMP_COLLECTORAPI_EVENT *) message;
  *pevent = event;

  char *mem = (char *)(message + sizeof(OMP_COLLECTORAPI_EVENT));
  if(append_func) {
         unsigned long *lmem = (unsigned long *) mem;
   /* need a better solution to eliminate compiler warning*/
   /*  (void (*)(OMP_COLLECTORAPI_EVENT))(*lmem) = func;  */
  }

     if(append_zero) {
       int *psz;
       if(append_func) {
            psz = (int *)(message+sizeof(OMP_COLLECTORAPI_EVENT)+ sizeof(void *)); 
   
     } else {

          psz = (int *)(message+sizeof(OMP_COLLECTORAPI_EVENT));

     }
       *psz =0;  
     } 
}


int init_collector(void)
{

  omp_collector_message req;
  void *message = (void *) malloc(4);   
  int *sz = (int *) message; 
  *sz = 0;

  /* first test: check for message size equal to 0 */ 
    __omp_collector_api(message);
 
   free(message);
  
   /*test: check for request start, 1 message */
   message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+sizeof(int));

   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_START, OMP_ERRCODE_OK, 0, 1);
   
   __omp_collector_api(message);

   free(message);

   /*test check for request stop, 1 message */
   message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+sizeof(int));

   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_STOP, OMP_ERRCODE_OK, 0, 1);
   
   __omp_collector_api(message);

   free(message);

    /*test check for request start, stop requests 2 messages in sequence*/
  
   message = (void *) malloc(2*OMP_COLLECTORAPI_HEADERSIZE+sizeof(int));

   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_START, OMP_ERRCODE_OK, 0, 0);
   fill_header(message+OMP_COLLECTORAPI_HEADERSIZE, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_STOP, OMP_ERRCODE_OK, 0, 1);

   __omp_collector_api(message);
 
   free(message);

    /*test check for request stop, start requests 2 messages in out of sequence*/
  
   message = (void *) malloc(2*OMP_COLLECTORAPI_HEADERSIZE+sizeof(int));

   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_STOP, OMP_ERRCODE_OK, 0, 0);
   fill_header(message+OMP_COLLECTORAPI_HEADERSIZE, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_START, OMP_ERRCODE_OK, 0, 1);

   __omp_collector_api(message);
   
   if ((int)*((int *)(message+sizeof(int)*2))!=OMP_ERRCODE_SEQUENCE_ERR) printf("runtime failed the out of sequence test"); 
   free(message);

      
    /*test check for register requests 1 for fork event after request start*/

   
   int register_sz = sizeof(OMP_COLLECTORAPI_EVENT)+sizeof(void *);
   message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+register_sz+sizeof(int)); 
   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE+register_sz, OMP_REQ_REGISTER, OMP_ERRCODE_OK, 0, 0);
   fill_register(message+OMP_COLLECTORAPI_HEADERSIZE,OMP_EVENT_FORK,1, dummyfunc, 1);

   __omp_collector_api(message);
   free(message);

   /*test check for unregister the fork event after request start */
    register_sz = sizeof(OMP_COLLECTORAPI_EVENT);
    message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+register_sz+sizeof(int));
    fill_header(message, OMP_COLLECTORAPI_HEADERSIZE+register_sz, OMP_REQ_UNREGISTER, OMP_ERRCODE_OK, 0, 0);
    fill_register(message+OMP_COLLECTORAPI_HEADERSIZE,OMP_EVENT_FORK,0,NULL, 1);
    __omp_collector_api(message);
    free(message);
   
    /*test for request of all events*/
   int i;
   int num_req=OMP_EVENT_THR_END_ATWT; /* last event */
   register_sz = sizeof(OMP_COLLECTORAPI_EVENT)+sizeof(void *);
   int mes_size = OMP_COLLECTORAPI_HEADERSIZE+register_sz;
   message = (void *) malloc(num_req*mes_size+sizeof(int));
   for(i=0;i<num_req;i++) {  
   fill_header(message+mes_size*i,mes_size, OMP_REQ_REGISTER, OMP_ERRCODE_OK, 0, 0);
   fill_register((message+mes_size*i)+OMP_COLLECTORAPI_HEADERSIZE,OMP_EVENT_FORK+i,1, dummyfunc, i==(num_req-1));
   } 
   __omp_collector_api(message);
   free(message);

   /*test for pause req*/
      
    message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+sizeof(int));

   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_PAUSE, OMP_ERRCODE_OK, 0, 1);
   
   __omp_collector_api(message);

   free(message);

   /* test for resume req */
    message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+sizeof(int));

   fill_header(message, OMP_COLLECTORAPI_HEADERSIZE, OMP_REQ_RESUME, OMP_ERRCODE_OK, 0, 1);
   
   __omp_collector_api(message);

   free(message);
   
   /* test for state query */
   int state_rsz = sizeof(OMP_COLLECTOR_API_THR_STATE)+sizeof(unsigned long);
    message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+state_rsz+sizeof(int));
    fill_header(message, OMP_COLLECTORAPI_HEADERSIZE+state_rsz, OMP_REQ_STATE, OMP_ERRCODE_OK, state_rsz, 1);
    __omp_collector_api(message);
    free(message);

    /* test for current parallel region id */
    int currentid_rsz = sizeof(long);
    message = (void *) malloc(OMP_COLLECTORAPI_HEADERSIZE+currentid_rsz+sizeof(int));
    fill_header(message, OMP_COLLECTORAPI_HEADERSIZE+currentid_rsz, OMP_REQ_CURRENT_PRID, OMP_ERRCODE_OK, currentid_rsz, 1);
    __omp_collector_api(message);
    free(message);  


    


  return 1;
}
