# ifndef __vector_H__
# define __vector_H__
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
typedef struct _vector *vector;

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

/* for test  */
EXTERN_ void      cv_info          (const vector cv                            );
EXTERN_ void      cv_print         (const vector cv                            );
#endif /* EOF file vector.h */
