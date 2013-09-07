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
    REAL frounding_error = 1.E-9;
    struct timeval tim;
    double start, end;
	
	NK = 2;
	NJ = 32;
    NI = 1<<20;
    
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
    
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);
  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   copyout(temp[0:NK*NJ*NI]) \
                       num_gangs(192) \
                       num_workers(8) \
                       vector_length(128) 
  {
    #pragma acc loop gang
    for(k=0; k<NK; k++)
	{
		#pragma acc loop worker
		for(j=0; j<NJ; j++)
		{
            REAL i_sum = 0.0;
			#pragma acc loop vector reduction(+:i_sum)
			for(i=0; i<NI; i++)
				i_sum += input[k*NJ*NI + j*NI + i];
            temp[k*NJ*NI + j*NI] = i_sum;
		}
	}
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);

	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
            REAL i_sum = 0.0;
			for(i=0; i<NI; i++)
				i_sum += input[k*NJ*NI + j*NI + i];

            if(fabsf(temp[k*NJ*NI + j*NI]-i_sum) > frounding_error)
            {
			    error++;
		        printf("vecotr + FAILED!\n");
            }
		}
	}
    
    printf("vector + execution time is :%.2lf: ms\n", end-start);
    if(error == 0)
        printf("vector + SUCCESS!\n");
    
    free(input);
    free(temp);
}
