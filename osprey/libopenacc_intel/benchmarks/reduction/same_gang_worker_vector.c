#include <stdio.h>
#include <stdlib.h>
#include <openacc.h>

int main()
{
	int i;
	int error;
	int NI;
	int *input;
    int sum;

	NI = 2048;

    acc_init(acc_device_default);
	input = (int*)malloc(NI*sizeof(int));

	for(i=0; i<NI; i++)
	{
	    input[i] = i%10;
	}

    sum = 0;
  #pragma acc parallel copyin(input[0:NI])
  {
    #pragma acc loop gang worker vector reduction(+:sum)
    for(i=0; i<NI; i++)
	{
	    sum += input[i];
	}
  }

	error = 0;
	
    int known_sum = 0;
	for(i=0; i<NI; i++)
	    known_sum += input[i];

	if(known_sum != sum)
		error++;

	free(input);

	printf("Test for same line gang worker vector reduction\n");
	if(error == 0)
		printf("SUCCESSFUL!\n");
	else
		printf("Reduction + FAILED! error=%d\n", error);

}
