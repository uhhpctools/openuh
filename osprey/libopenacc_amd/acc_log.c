/**
 * Author: Rengan Xu
 * University of Houston
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "acc_log.h"

static FILE* _log_file = NULL;
static int _acc_error_handler_called_already = 0;

void acc_error_handler(const char* format, ... )
{
    va_list args;
    FILE* log_file = _log_file == NULL ? stderr : _log_file;
    
    if(_acc_error_handler_called_already) return;
    _acc_error_handler_called_already = 1;

    fflush(stdout);
    fflush(stderr);

    if(format != NULL)
    {
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
    }
    
    fputc('\n', log_file);

    exit(EXIT_FAILURE);
} 

void acc_log_handler(const char* format, ... )
{
    va_list args;
    FILE* log_file = _log_file == NULL ? stderr : _log_file;
   
    if(format != NULL)
    {
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
    } 
    
    fputc('\n', log_file);
}

void _acc_log_lock()
{
}

void _acc_log_unlock()
{
}


