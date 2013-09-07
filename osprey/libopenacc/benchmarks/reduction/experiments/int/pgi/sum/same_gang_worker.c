
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <openacc.h>
#include <sys/time.h>

#define TYPE int

int main()
{
	int i, j, k;
	TYPE sum, known_sum;
	int NI;
    int error;
	TYPE *input;
    struct timeval tim;
    double start, end;
    
	NI = 1<<20;

    error = 0;
	input = (TYPE*)malloc(NI*sizeof(TYPE));
    acc_init(acc_device_default);

    srand((unsigned)time(0));
	for(i=0; i<NI; i++)
	{
		input[i] = rand()%5 + 1;
	}
	
    sum = 0;
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
  #pragma acc parallel copyin(input[0:NI]) \
                       num_gangs(192) \
                       num_workers(8) \
                       vector_length(32) 
  {
    #pragma acc loop gang worker reduction(+:sum)
	for(i=0; i<NI; i++)
	    sum += input[i];
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
	
    known_sum = 0;
	for(i=0; i<NI; i++)
	{
		known_sum += input[i];
	}
	
    if(sum != known_sum)
    {
        error++;
		printf("same_gang_worker + FAILED! sum=%d, known_sum=%d\n", sum, known_sum);
    }
    
    printf("same_gang_worker + execution time is :%.2lf: ms\n", end-start);

    if(error == 0)
        printf("same_gang_worker + SUCCESS!\n");

    free(input);
}
