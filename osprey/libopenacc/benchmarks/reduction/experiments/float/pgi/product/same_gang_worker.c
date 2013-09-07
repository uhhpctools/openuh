
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <openacc.h>
#include <sys/time.h>

#define TYPE float

int main()
{
	int i, j, k;
	TYPE product, known_product;
	int NI;
    int error;
	TYPE *input;
    TYPE rounding_error = 1.E-9;
    struct timeval tim;
    double start, end;
    
	NI = 1<<20;

    error = 0;
	input = (TYPE*)malloc(NI*sizeof(TYPE));
    acc_init(acc_device_default);

    srand((unsigned)time(0));
	for(i=0; i<NI; i++)
	{
		input[i] = (TYPE)rand()/(TYPE)RAND_MAX + 0.1;
	}
	
    product = 1;
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
  #pragma acc parallel copyin(input[0:NI]) \
                       num_gangs(192) \
                       num_workers(8) \
                       vector_length(32) 
  {
    #pragma acc loop gang worker reduction(*:product)
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
	
    if(fabsf(product - known_product) > rounding_error)
    {
        error++;
		printf("same_gang_worker + FAILED! product=%d, known_product=%d\n", product, known_product);
    }
    
    printf("same_gang_worker + execution time is :%.2lf: ms\n", end-start);

    if(error == 0)
        printf("same_gang_worker + SUCCESS!\n");

    free(input);
}
