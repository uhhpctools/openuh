
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
	TYPE product, known_product;
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
	
    product = 1;
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
  #pragma acc parallel copyin(input[0:NI]) \
                       num_gangs(1) \
                       num_workers(8) \
                       vector_length(128) 
  {
    #pragma acc loop worker vector reduction(*:product)
	for(i=0; i<NI; i++)
	    product *= input[i];
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
	
    known_product = 1;
	for(i=0; i<NI; i++)
	{
		known_product *= input[i];
	}
	
    if(product != known_product)
    {
        error++;
		printf("same_worker_vector + FAILED! product=%d, known_product=%d\n", product, known_product);
    }
    
    printf("same_worker_vector + execution time is :%.2lf: ms\n", end-start);

    if(error == 0)
        printf("same_worker_vector + SUCCESS!\n");

    free(input);
}
