# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# define MIN_LEN 256
# define CVEFAILED  -1
# define CVESUCCESS  0
# define CVEPUSHBACK 1
# define CVEPOPBACK  2
# define CVEINSERT   3
# define CVERM       4
# define EXPANED_VAL 1
# define REDUSED_VAL 2

typedef void *citerator;
typedef struct _vector 
{     
	void *cv_pdata;
	size_t cv_len, cv_tot_len, cv_size;
} *vector;

# define CWARNING_ITER(cv, iter, file, func, line) \
	do {\
		if ((vector_begin(cv) > iter) || (vector_end(cv) <= iter)) {\
			fprintf(stderr, "var(" #iter ") warng out of range, "\
					"at file:%s func:%s line:%d!!/n", file, func, line);\
			return CVEFAILED;\
		}\
	} while (0)

# ifdef _cplusplus
# define EXTERN_ extern "C"
# else
# define EXTERN_ extern
# endif

EXTERN_ vector    vector_create   (const size_t size                          );
EXTERN_ void      vector_destroy  (const vector cv                            );
EXTERN_ size_t    vector_length   (const vector cv                            );
EXTERN_ int       vector_pushback (const vector cv, void *memb                );
EXTERN_ int       vector_popback  (const vector cv, void *memb                );
EXTERN_ size_t    vector_iter_at  (const vector cv, citerator iter            );
EXTERN_ int       vector_iter_val (const vector cv, citerator iter, void *memb);
EXTERN_ citerator vector_begin    (const vector cv                            );
EXTERN_ citerator vector_end      (const vector cv                            );
EXTERN_ citerator vector_next     (const vector cv, citerator iter            );
EXTERN_ int       vector_val_at   (const vector cv, size_t index, void *memb  );
EXTERN_ int       vector_insert   (const vector cv, citerator iter, void *memb);
EXTERN_ int       vector_insert_at(const vector cv, size_t index, void *memb  );
EXTERN_ int       vector_rm       (const vector cv, citerator iter            );
EXTERN_ int       vector_rm_at    (const vector cv, size_t index              );

vector vector_create(const size_t size)
{
	vector cv = (vector)malloc(sizeof (struct _vector));

	if (!cv) return NULL;

	cv->cv_pdata = malloc(MIN_LEN * size);

	if (!cv->cv_pdata) 
	{
		free(cv);
		return NULL;
	}

	cv->cv_size = size;
	cv->cv_tot_len = MIN_LEN;
	cv->cv_len = 0;

	return cv;
}

void vector_destroy(const vector cv)
{
	free(cv->cv_pdata);
	free(cv);
	return;
}

size_t vector_length(const vector cv)
{
	return cv->cv_len;
}

int vector_pushback(const vector cv, void *memb)
{
	if (cv->cv_len >= cv->cv_tot_len) 
	{
		void *pd_sav = cv->cv_pdata;
		cv->cv_tot_len <<= EXPANED_VAL;
		cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

		if (!cv->cv_pdata) 
		{
			cv->cv_pdata = pd_sav;
			cv->cv_tot_len >>= EXPANED_VAL;
			return CVEPUSHBACK;
		}
	}

	memcpy(cv->cv_pdata + cv->cv_len * cv->cv_size, memb, cv->cv_size);
	cv->cv_len++;

	return CVESUCCESS;
}

int vector_popback(const vector cv, void *memb)
{
	if (cv->cv_len <= 0) return CVEPOPBACK;

	cv->cv_len--;
	memcpy(memb, cv->cv_pdata + cv->cv_len * cv->cv_size, cv->cv_size);

	if ((cv->cv_tot_len >= (MIN_LEN << REDUSED_VAL)) 
			&& (cv->cv_len <= (cv->cv_tot_len >> REDUSED_VAL))) 
	{
		void *pd_sav = cv->cv_pdata;
		cv->cv_tot_len >>= EXPANED_VAL;
		cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

		if (!cv->cv_pdata) 
		{
			cv->cv_tot_len <<= EXPANED_VAL;
			cv->cv_pdata = pd_sav;
			return CVEPOPBACK;
		}
	}

	return CVESUCCESS;
}

size_t vector_iter_at(const vector cv, citerator iter)
{
	CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
	return (iter - cv->cv_pdata) / cv->cv_size;
}

int vector_iter_val(const vector cv, citerator iter, void *memb)
{
	CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
	memcpy(memb, iter, cv->cv_size);
	return 0;
}

citerator vector_begin(const vector cv)
{
	return cv->cv_pdata;
}

citerator vector_end(const vector cv)
{
	return cv->cv_pdata + (cv->cv_size * cv->cv_len);
}

static inline void cvmemove_foreward(const vector cv, void *from, void *to)
{
	size_t size = cv->cv_size;
	void *p;
	for (p = to; p >= from; p -= size) memcpy(p + size, p, size);
	return;
}

static inline void cvmemove_backward(const vector cv, void *from, void *to)
{
	memcpy(from, from + cv->cv_size, to - from);
	return;
}

int vector_insert(const vector cv, citerator iter, void *memb)
{
	CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);

	if (cv->cv_len >= cv->cv_tot_len) 
	{
		void *pd_sav = cv->cv_pdata;
		cv->cv_tot_len <<= EXPANED_VAL;
		cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

		if (!cv->cv_pdata) 
		{
			cv->cv_pdata = pd_sav;
			cv->cv_tot_len >>= EXPANED_VAL;
			return CVEINSERT;
		}
	}

	cvmemove_foreward(cv, iter, cv->cv_pdata + cv->cv_len * cv->cv_size);
	memcpy(iter, memb, cv->cv_size);
	cv->cv_len++;

	return CVESUCCESS;
}

int vector_insert_at(const vector cv, size_t index, void *memb)
{
	citerator iter;

	if (index >= cv->cv_tot_len) 
	{
		cv->cv_len = index + 1;
		while (cv->cv_len >= cv->cv_tot_len) cv->cv_tot_len <<= EXPANED_VAL;
		cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);
		iter = cv->cv_pdata + cv->cv_size * index;
		memcpy(iter, memb, cv->cv_size);
	}
	else 
	{
		iter = cv->cv_pdata + cv->cv_size * index;
		vector_insert(cv, iter, memb);
	}

	return 0;
}

citerator vector_next(const vector cv, citerator iter)
{
	return iter + cv->cv_size;
}

int vector_val(const vector cv, citerator iter, void *memb)
{
	memcpy(memb, iter, cv->cv_size);
	return 0;
}

int vector_val_at(const vector cv, size_t index, void *memb)
{
	memcpy(memb, cv->cv_pdata + index * cv->cv_size, cv->cv_size);
	return 0;
}

int vector_rm(const vector cv, citerator iter)
{
	citerator from;
	citerator end;
	CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
	from = iter;
	end = vector_end(cv);
	memcpy(from, from + cv->cv_size, end - from);
	cv->cv_len--;

	if ((cv->cv_tot_len >= (MIN_LEN << REDUSED_VAL))
			&& (cv->cv_len <= (cv->cv_tot_len >> REDUSED_VAL))) 
	{
		void *pd_sav = cv->cv_pdata;
		cv->cv_tot_len >>= EXPANED_VAL;
		cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

		if (!cv->cv_pdata) 
		{
			cv->cv_tot_len <<= EXPANED_VAL;
			cv->cv_pdata = pd_sav;
			return CVERM;
		}
	}

	return CVESUCCESS;
}

int vector_rm_at(const vector cv, size_t index)
{
	citerator iter;
	iter = cv->cv_pdata + cv->cv_size * index;
	CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
	return vector_rm(cv, iter);
}

void cv_info(const vector cv)
{
	printf("\n\nntot :%s : %zd\n", __func__, cv->cv_tot_len);
	printf("len :%s : %zd\n",     __func__, cv->cv_len);
	printf("size:%s : %zd\n\n",   __func__, cv->cv_size);
	return;
}

/*This is just a example, the data type in this example is int, the data type can also be a struct*/
void cv_print(const vector cv)
{
	int num;
	citerator iter;

	if (vector_length(cv) == 0)
		fprintf(stderr, "file:%s func:%s line:%d error, null length vector!!\n", __FILE__, __func__, __LINE__);

	for (iter = vector_begin(cv); 
			iter != vector_end(cv);
			iter = vector_next(cv, iter)) 
	{
		vector_iter_val(cv, iter, &num);
		printf("var:%d at:%zd\n", num, vector_iter_at(cv, iter));
	}

	return;
}
