#include <stdio.h>
#include <stdlib.h>
#include <openacc.h>
#include <sys/time.h>


int main()
{
	int i, j, k;
	int sum, known_sum;
	int NI, NJ, NK;
	int *input, *temp;
    struct timeval tim;
    double start, end;

	NI = 1024;
	NJ = 512;
	NK = 128;

	input = (int*)malloc(NK*NJ*NI*sizeof(int));
	temp = (int*)malloc(NK*NJ*NI*sizeof(int));

    acc_init(acc_device_default);
    /*1. test for reduction + */
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				input[k*NJ*NI + j*NI + i] = (k+j+i)%10;
		}
	}

	sum = 0;

    gettimeofday(&tim, NULL);
    start = tim.tv_sec*1000 + (tim.tv_usec/1000.0);

  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   create(temp[0:NK*NJ*NI])
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
    
	known_sum = 0;
	for(k=0; k<NK; k++)
	{
		known_sum += input[k*NJ*NI];
	}

	printf("Test for gang reduction \n");
	if(sum == known_sum)
		printf("Success!\n");
	else
		printf("Int + failed! sum=%d, known_sum=%d\n", sum, known_sum);

    printf("Execution time: %.2lf ms\n", end- start);
#if 0   
    /*2. test for reduction * */ 
	for(k=0; k<NK; k++)
	{
		for(j=0; j<NJ; j++)
		{
			for(i=0; i<NI; i++)
				input[k*NJ*NI + j*NI + i] = (k+j+i+3)%5;
		}
	}

  int product = 1;
  #pragma acc parallel copyin(input[0:NK*NJ*NI]) \
  					   create(temp[0:NK*NJ*NI])
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

    int known_product = 1;
	for(k=0; k<NK; k++)
	{
		known_product *= input[k*NJ*NI];
	}
	if(product == known_product)
		printf("Success!\n");
	else
		printf("Int * failed! product=%d, known_product=%d\n", product, known_product);
#endif
	free(input);
	free(temp);
}
