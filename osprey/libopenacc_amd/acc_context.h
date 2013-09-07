/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "acc_common.h"
#include "acc_data.h"

extern void __accr_setup(void);

extern void acc_init(acc_device_t);

extern void __accr_cleanup(void);

extern void acc_shutdown(acc_device_t);

#endif
