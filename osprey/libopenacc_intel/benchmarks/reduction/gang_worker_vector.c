#include <stdio.h>
#include <stdlib.h>
#include <openacc.h>


int main()
{
	int i, j, k;
	int sum, known_sum;
	int NI, NJ, NK;
	int *input;

	NI = 1024;
	NJ = 512;
	NK = 128;

    acc_init(acc_device_default);

	input = (int*)malloc(NK*NJ*NI*sizeof(int));

	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				input[k*NJ*NI + j*NI + i] = (k+j+i)%10;
		}
	}

	sum = 0;
  #pragma acc parallel copyin(input[0:NK*NJ*NI])
  {
    #pragma acc loop gang reduction(+:sum)
    for(k=0; k<NK; k++)
	{
		#pragma acc loop worker
		//#pragma acc loop worker reduction(+:sum)
		for(j=0; j<NJ; j++)
		{
			#pragma acc loop vector
			//#pragma acc loop vector reduction(+:sum)
			for(i=0; i<NI; i++)
				sum += input[k*NJ*NI + j*NI + i];
		}
	}
  }

	known_sum = 0;
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				known_sum += input[k*NJ*NI + j*NI + i];
		}
	}

	free(input);
    printf("Test for reduction in gang, worker and vector\n");
	if(sum == known_sum)
		printf("Success!\n");
	else
		printf("Reduction + Failed! sum=%d, known_sum=%d\n", sum, known_sum);
}
