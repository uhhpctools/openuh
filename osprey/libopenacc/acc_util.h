/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */
 
#ifndef _CUTIL_INLINE_FUNCTIONS_DRVAPI_H_
#define _CUTIL_INLINE_FUNCTIONS_DRVAPI_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cuda.h>
#include <cuda_runtime.h>

// Error Code string definitions here
typedef struct
{
    char const *error_string;
    int  error_id;
} s_CudaErrorStr;


// This is just a linear search through the array, since the error_id's are not
// always ocurring consecutively
extern inline const char * getCudaDrvErrorString(CUresult error_id);

extern inline void __cuSafeRTCall(cudaError_t err, const char *file, const int line);

// We define these calls here, so the user doesn't need to include __FILE__ and __LINE__
// The advantage is the developers gets to use the inline function so they can debug
#define cutilRTSafeCall(err)            __cuSafeRTCall      (err, __FILE__, __LINE__)
#define CUDART_CHECK(err)               __cuSafeRTCall      (err, __FILE__, __LINE__)
#define cutilDrvSafeCallNoSync(err)     __cuSafeCallNoSync  (err, __FILE__, __LINE__)
#define cutilDrvSafeCall(err)           __cuSafeCall        (err, __FILE__, __LINE__)
#define CUDA_CHECK(err)                 __cuSafeCall        (err, __FILE__, __LINE__)
#define cutilDrvCtxSync()               __cuCtxSync         (__FILE__, __LINE__)
#define cutilDrvCheckMsg(msg)           __cuCheckMsg        (msg, __FILE__, __LINE__)
#define cutilDrvAlignOffset(offset, alignment)  ( offset = (offset + (alignment-1)) & ~((alignment-1)) )

// These are the inline versions for all of the CUTIL functions
extern inline void __cuSafeCallNoSync( CUresult err, const char *file, const int line );

extern inline void __cuSafeCall( CUresult err, const char *file, const int line );

extern inline void __cuCtxSync(const char *file, const int line );


// This function returns the best Graphics GPU based on performance

extern inline void __cuCheckMsg( const char * msg, const char *file, const int line );


#endif // _CUTIL_INLINE_FUNCTIONS_DRVAPI_H_
