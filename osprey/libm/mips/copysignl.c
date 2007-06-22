
extern	double	__copysign(double, double);

#ifdef __GNUC__
extern  long double  __copysignl(long double, long double);

long double    copysignl() __attribute__ ((weak, alias ("__copysignl")));

#endif

long double
__copysignl(long double x, long double y)
{
	return ( (long double)__copysign((double)x, (double)y) );
}

