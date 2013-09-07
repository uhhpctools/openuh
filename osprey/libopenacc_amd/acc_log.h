/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __ACC_LOG_H__
#define __ACC_LOG_H__

#ifdef ERROR
#undef ERROR
#endif

#ifdef WARN
#undef WARN
#endif

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef INFO
#undef INFO
#endif

#ifdef _LOG
#undef _LOG
#endif

#define NOOP do{;}while(0);

typedef enum{
    ACC_LOG_ERROR = 0,
    ACC_LOG_WARN  = 1,
	ACC_LOG_INFO  = 2,
    ACC_LOG_DEBUG = 3
}acc_log_type;

void acc_error_handler(const char* format, ... );
void acc_log_handler(const char* format, ... );

void _acc_log_lock();
void _acc_log_unlock();

#ifdef ACC_LOG

#define _LOG(t, h, f)                           \
    do {                                        \
        if(t == ACC_LOG_ERROR){                 \
            _acc_log_lock();                    \
            h f;                                \
            _acc_log_unlock();                  \
        }                                       \
    }while(0)

#else

#define _LOG(t, h, f)               \
    do {                            \
        _acc_log_lock();            \
        h f;                        \
        _acc_log_unlock();          \
    }while(0)

#endif

#define ERROR(f) _LOG(ACC_LOG_ERROR, acc_error_handler, f)

#ifdef ACC_WARN_LOG
#  define WARN(f) _LOG(ACC_LOG_WARN, acc_log_handler, f)
#else
#  define WARN(f) NOOP
#endif

#ifdef ACC_DEBUG_LOG
#  define DEBUG(f) _LOG(ACC_LOG_DEBUG, acc_log_handler, f)
#else
#  define DEBUG(f) NOOP
#endif

#ifdef ACC_INFO_LOG
#  define INFO(f) _LOG(ACC_LOG_INFO, acc_log_handler, f)
#else
#  define INFO(f) NOOP
#endif

#endif
