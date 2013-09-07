/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __DATA_H__
#define __DATA_H__

#include "acc_common.h"
#include "acc_kernel.h"
#include "acc_data.h"

typedef struct acc_hm_entry_s acc_hm_entry;

struct param_s{
	void *host_addr;
	void *device_addr;
	size_t size;
    int pinned;
	//TRANSFER_TYPE type; /*indicate in/out from device*/
};

extern vector param_list;
extern acc_hashmap* map;
extern int MODULE_BASE;

#endif
