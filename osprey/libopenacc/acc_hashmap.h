/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __ACC_HASHMAP_H
#define __ACC_HASHMAP_H

#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include "acc_log.h"

typedef void* acc_hash;

typedef struct param_s param_t;

struct acc_hm_entry_s {
    const char* string_key;
    int key;
    param_t *value;
    int hash;
    struct acc_hm_entry_s* next;
};

typedef struct acc_hashmap_s{
    /* the number of key-value mappings in this hashmap */
    int size;

    int capacity;
    float load_factor;
    /*
     * threshold (=capacity*load_factor), the table is resized 
     * if its size is larger than this threshold
     */
    int threshold;

    struct acc_hm_entry_s** table;  
}acc_hashmap;

acc_hash acc_hashmap_create();

acc_hash acc_hashmap_create_with_args(int initial_capacity, float load_factor);

/* insert a key-map pair entry into the hashmap */
param_t* acc_hashmap_put(void* _hm, int key, param_t* value);

param_t* acc_hashmap_put_string(void* _hm, const char* string_key, param_t* value);

/* get a key-value entry from the hashmap based on the key value */
param_t* acc_hashmap_get(void* _hm, int key);

/* get the key value based on the index */
param_t* acc_hashmap_get_from_index(void* _hm, int index, void* pDevice);

param_t* acc_hashmap_get_string(void* _hm, const char* string_key);

/* remove a key-value entry from the hashmap based on the key value */
void* acc_hashmap_remove(void* _hm, int key);

/* remove all key-value mappings in this hashmap */
void acc_hashmap_clear(void* _hm);

/* destroy the hashmap */
void acc_hashmap_destroy(void* _hm);

#endif
