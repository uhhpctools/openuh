#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <openacc.h>
#include <sys/time.h>

int main()
{
	int i, j, k;
	int sum, known_sum;
	int NI, NJ, NK;
    int error;
	int *input, *temp;
    struct timeval tim;
    double start, end;
	
    NK = 2;
	NJ = 1<<20;
	NI = 32;
    
    error = 0;

	input = (int*)malloc(NK*NJ*NI*sizeof(int));
	temp = (int*)malloc(NK*NJ*NI*sizeof(int));
	
    acc_init(acc_device_default);
    
    srand((unsigned)time(0));
    /*1. test for reduction + */
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
            {
				input[k*NJ*NI + j*NI + i] = rand()%5 + 1;
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
        int j_sum = 0;
		#pragma acc loop worker reduction(+:j_sum)
		for(j=0; j<NJ; j++)
		{
			#pragma acc loop vector
			for(i=0; i<NI; i++)
				temp[k*NJ*NI + j*NI + i] = input[k*NJ*NI + j*NI + i];
            j_sum += temp[k*NJ*NI + j*NI];
		}
        j_sum += k;
	    temp[k*NJ*NI] = j_sum;
	}
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);

	for(k=0; k<NK; k++)
	{
		int j_sum = k;
		for(j=0; j<NJ; j++)
		{
			j_sum += input[k*NJ*NI + j*NI + 0];
		}
		if(temp[k*NJ*NI] != j_sum)
        {
			error++;
		    printf("worker + FAILED!\n");
        }
	}

    printf("worker + execution time is :%.2lf: ms\n", end-start);
    if(error == 0)
        printf("worker + SUCCESS!\n");

	free(input);
	free(temp);

    return 1;
}
