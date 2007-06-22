
extern	double	__floor(double);

#ifdef __GNUC__
extern  long double  __floorl(long double);

long double    floorl() __attribute__ ((weak, alias ("__floorl")));

#endif

long double
__floorl(long double x)
{
	return ( (long double)__floor((double)x) );
}

