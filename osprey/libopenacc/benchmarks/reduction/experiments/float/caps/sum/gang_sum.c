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
	int NI, NJ, NK;
    int error;
    REAL sum, known_sum;
	REAL *input, *temp;
    REAL rounding_error = 1.E-9;
    struct timeval tim;
    double start, end;
	
    NK = 1<<20;
	NJ = 2;
	NI = 32;
    
    error = 0;
	input = (REAL*)malloc(NK*NJ*NI*sizeof(REAL));
	temp = (REAL*)malloc(NK*NJ*NI*sizeof(REAL));
    
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

    sum = 0.0;
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   create(temp[0:NK*NJ*NI]) \
                       num_gangs(192) \
                       num_workers(8) \
                       vector_length(128) 
  {
    #pragma acc loop gang reduction(+:sum)
    for(k=0; k<NK; k++)
	{
		#pragma acc loop worker
		for(j=0; j<NJ; j++)
		{
			#pragma acc loop vector
			for(i=0; i<NI; i++)
				temp[k*NJ*NI + j*NI + i] = input[k*NJ*NI + j*NI + i];
		}
		sum += temp[k*NJ*NI];
	}
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);

    known_sum = 0.0;
	for(k=0; k<NK; k++)
	{
		known_sum += input[k*NJ*NI];
	}

	if(fabsf(sum-known_sum) > rounding_error)
    {
        error++;
		printf("gang + FAILED! sum=%f, known_sum=%f\n", sum, known_sum);
    }
    printf("gang + execution time is :%.2lf: ms\n", end-start);

    if(error == 0)
        printf("gang + SUCCESS!\n");
	
    free(input);
	free(temp);
    return 0;
}
