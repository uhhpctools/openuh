#ifndef	_OMP_COLLECTOR_UTIL_H
#define	_OMP_COLLECTOR_UTIL_H


#ifdef __cplusplus
extern "C" {
#endif

extern char OMP_EVENT_NAME[22][50];
extern char OMP_STATE_NAME[11][50];

int __omp_collector_api(void *arg);

void __omp_collector_init(void);
    
#ifdef __cplusplus
}
#endif

#endif	/* _OMP_COLLECTOR_UTIL_H */

