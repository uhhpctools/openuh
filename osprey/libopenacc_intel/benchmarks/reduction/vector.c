#include <stdio.h>
#include <stdlib.h>
#include <openacc.h>
#include <sys/time.h>


int main()
{
	int i, j, k;
	int error;
	int NI, NJ, NK;
	int *input, *temp;
    struct timeval tim;
    double start, end;

	NI = 1024;
	NJ = 512;
	NK = 128;

    acc_init(acc_device_default);
	input = (int*)malloc(NK*NJ*NI*sizeof(int));
	temp = (int*)malloc(NK*NJ*NI*sizeof(int));

	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				input[k*NJ*NI + j*NI + i] = (k+j+i)%10;
		}
	}
    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);

  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   copyout(temp[0:NK*NJ*NI])
  {
    #pragma acc loop gang
    for(k=0; k<NK; k++)
	{
		#pragma acc loop worker 
		for(j=0; j<NJ; j++)
		{
			int i_sum = 0;
			#pragma acc loop vector reduction(+:i_sum)
			for(i=0; i<NI; i++)
				i_sum += input[k*NJ*NI + j*NI + i];
            i_sum += j;
			temp[k*NJ*NI + j*NI] = i_sum;
		}
	}
  }
    gettimeofday(&tim, NULL);
    end = tim.tv_sec*1000 + (tim.tv_usec/1000.0);

	error = 0;
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			int i_sum = j;
			for(i=0; i<NI; i++)
				i_sum += input[k*NJ*NI + j*NI + i];

			if(temp[k*NJ*NI +j*NI] != i_sum)
				error++;
		}
	}

	free(input);
	free(temp);
	printf("Test for vector reduction\n");
	if(error == 0)
		printf("SUCCESSFUL!\n");
	else
		printf("Reduction + FAILED! error=%d\n", error);

    printf("Execution time: %.2lf ms\n", end- start);
}
