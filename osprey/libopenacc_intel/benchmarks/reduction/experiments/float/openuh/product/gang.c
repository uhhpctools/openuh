#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <openacc.h>
#include <sys/time.h>

#define REAL float

int main()
{
	int i, j, k;
	int sum, known_sum;
	int NI, NJ, NK;
    int error;
	REAL *input, *temp;
    REAL rounding_error = 1.E-9;
    struct timeval tim;
    double start, end;
	
    NK = 1<<20;
	NJ = 2;
	NI = 32;
    
    input = (REAL*)malloc(NK*NJ*NI*sizeof(REAL));
	temp = (REAL*)malloc(NK*NJ*NI*sizeof(REAL));
   
    error = 0; 
    acc_init(acc_device_default);

    srand((unsigned)time(0));
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
            {
				input[k*NJ*NI + j*NI + i] = (REAL)rand()/(REAL)RAND_MAX + 0.1;
            }
		}
	}
  
    REAL product = 1;
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   create(temp[0:NK*NJ*NI]) \
                       num_gangs(192) \
                       num_workers(8) \
                       vector_length(128) 
  {
    #pragma acc loop gang reduction(*:product)
    for(k=0; k<NK; k++)
	{
		#pragma acc loop worker
		for(j=0; j<NJ; j++)
		{
			#pragma acc loop vector
			for(i=0; i<NI; i++)
				temp[k*NJ*NI + j*NI + i] = input[k*NJ*NI + j*NI + i];
		}
		product *= temp[k*NJ*NI];
	}
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
    
    REAL known_product = 1;
	for(k=0; k<NK; k++)
		known_product *= input[k*NJ*NI];
	if(fabsf(product - known_product) > rounding_error)
    {
        error++;
		printf("gang * FAILED! product=%d, known_product=%d\n", product, known_product);
    }
        
    printf("gang * execution time is :%.2lf: ms\n", end-start);

    if(error == 0)
        printf("gang * SUCCESS!\n");
}
