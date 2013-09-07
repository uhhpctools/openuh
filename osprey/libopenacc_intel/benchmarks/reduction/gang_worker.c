#include <stdio.h>
#include <stdlib.h>
#include <openacc.h>

int main()
{
	int i, j, k;
	int sum, known_sum;
	int NI, NJ, NK;
	int *input, *temp;

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

	sum = 0;
  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
                       create(temp[0:NK*NJ*NI])
  {
    #pragma acc loop gang reduction(+:sum)
    for(k=0; k<NK; k++)
	{
		//#pragma acc loop worker reduction(+:sum)
		#pragma acc loop worker
		for(j=0; j<NJ; j++)
		{
			#pragma acc loop vector
			for(i=0; i<NI; i++)
				temp[k*NJ*NI + j*NI + i] = input[k*NJ*NI + j*NI + i];
            
            sum += temp[k*NJ*NI + j*NI];
		}
	}
  }

	known_sum = 0;
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				temp[k*NJ*NI + j*NI + i] = input[k*NJ*NI + j*NI + i];
            known_sum += temp[k*NJ*NI + j*NI];
		}
	}

	free(input);
    free(temp);
    printf("Test for reduction in gang and worker\n");
	if(sum == known_sum)
		printf("Success!\n");
	else
		printf("Reduction + Failed! sum=%d, known_sum=%d\n", sum, known_sum);
}
