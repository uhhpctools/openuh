
extern	double	__rint(double);

#ifdef __GNUC__
extern  long double  __rintl(long double);

long double    rintl() __attribute__ ((weak, alias ("__rintl")));

#endif

long double
__rintl(long double x)
{
	return ( (long double)__rint((double)x) );
}

