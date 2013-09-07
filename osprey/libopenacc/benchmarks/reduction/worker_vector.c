#include <stdio.h>
#include <stdlib.h>
#include <openacc.h>


int main()
{
	int i, j, k;
	int sum, known_sum;
	int NI, NJ, NK;
	int *input, *temp;
	int error;

	NI = 1024;
	NJ = 512;
	NK = 128;

    acc_init(acc_device_default);
	input = (int*)malloc(NK*NJ*NI*sizeof(int));
	temp = (int*)malloc(NK*sizeof(int));

	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				input[k*NJ*NI + j*NI + i] = (k+j+i)%10;
		}
	}

  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   copyout(temp[0:NK])
  {
    #pragma acc loop gang
    for(k=0; k<NK; k++)
	{
		//int j_sum = k;
		int j_sum = 0;
		#pragma acc loop worker reduction(+:j_sum)
		for(j=0; j<NJ; j++)
		{
			//#pragma acc loop vector reduction(+:j_sum)
			#pragma acc loop vector
			for(i=0; i<NI; i++)
				j_sum += input[k*NJ*NI + j*NI + i];
		}
        j_sum += k;
		temp[k] = j_sum;
	}
  }

	error = 0;
	for(k=0; k<NK; k++)
	{
		int j_sum = k;
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				j_sum += input[k*NJ*NI + j*NI + i];
		}
		if(temp[k] != j_sum)
			error++;
	}

	free(input);
	free(temp);
	printf("Test for worker vector reduction\n");
	if(error == 0)
		printf("SUCCESSFUL!\n");
	else
		printf("Reduction + FAILED! error=%d\n", error);
}
