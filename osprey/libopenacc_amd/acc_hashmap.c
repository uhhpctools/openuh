/**
 * Author: Rengan Xu
 * University of Houston
 */

/**
 * This implementation is based on the Java hash map implementation.
 * This implementation provides constant-time performance for the basic
 * operations (get and put). Iteration over collection views requires
 * time proportional to the "capacity" of the HashMap instance (the number
 * of buckets) plus its size (key-value mappings). Thus, it is very important
 * not to set the initial capacity too high (or the load factor too low) if
 * iteration performance is important.
 *
 * An instance of HashMap has two parameters that affects its performance:
 * initial_capacity and load factor. The capacity is the number of buckets
 * in the hash table. Load factor is a measurement of how full the hash table
 * is allowed to get before the capacity is automatically increased. When the
 * number of entries exceeds the threshold (the product of capacity and load factor)
 * the hash table is rehashed (that is, the internal data structures are rebuilt)
 * so that the hash table is approximately twice the number of buckets.
 *
 * Each bucket may contain multiple entries who link together, the optimal 
 * performance can be obtained when each bucket has only one entry. Whenever 
 * insert an entry, the entry is place at the head of the entry list in that bucket.
 */

#include "acc_hashmap.h"
#include "acc_data.h"

#define HM_INITIAL_CAPACITY 16
#define HM_LOAD_FACTOR 0.75
#define HM_MAXIMUM_CAPACITY (1<<30)

static int hash(int h);

static int hash_string(const char* word);

static int indexFor(int h, int length);

static void addEntry(acc_hashmap* hm, int key, param_t* value, int hashcode, int i);

static void resize(acc_hashmap* hm, int new_capactiy);

acc_hash acc_hashmap_create()
{
    return acc_hashmap_create_with_args(HM_INITIAL_CAPACITY, HM_LOAD_FACTOR);   
}

acc_hash acc_hashmap_create_with_args(int initial_capacity, float load_factor)
{
    acc_hashmap* hashmap;
    int capacity;

    if(initial_capacity < 0)
        ERROR(("Illegal initial capacity: %d", initial_capacity));
    if(initial_capacity > HM_MAXIMUM_CAPACITY)
        initial_capacity = HM_MAXIMUM_CAPACITY;

    if(load_factor <= 0 || isnan(load_factor))
        ERROR(("Illegal load factor: %f", load_factor));
        
    /* compute the minimum power-of-2 value greater than initial_capacity */
    capacity = 1;
    while(capacity < initial_capacity)
        capacity <<= 1;

    hashmap = (acc_hashmap*)malloc(sizeof(acc_hashmap));
    
    hashmap->size = 0;
    hashmap->load_factor = load_factor;
    hashmap->threshold = (int)(capacity*load_factor);
    hashmap->table = (acc_hm_entry**)malloc(capacity * sizeof(acc_hm_entry*));
    memset(hashmap->table, 0, capacity*sizeof(acc_hm_entry*));
    hashmap->capacity = capacity;

    return (acc_hash)hashmap;
}

static int hash(int h)
{
    unsigned int x = h;
    x ^= (x >> 20) ^ (x >> 12);
    return (x ^ (x >> 7) ^ (x >> 4));
}

/* 
 * assume the length of string word is n, then the hashcode formula is
 * word[0]*31^(n-1) + word[1]*31^(n-2) + ... + word[n-1]
 *
 */
static int hash_string(const char* word)
{
    int i, hash;
   
    hash = 0; 
    for(i=0; word[i] != '\0'; i++)
    {
        hash = 31*hash + word[i];
    }

    return hash;
}

static int indexFor(int h, int length)
{
    return h & (length - 1);
}

static void addEntry(acc_hashmap* hm, int key, param_t* value, int hashcode, int i)
{
    acc_hm_entry* entry = (acc_hm_entry*)malloc(sizeof(acc_hm_entry));
    memset(entry, 0, sizeof(acc_hm_entry));   
 
    entry->key = key;
    entry->value = value;    
    entry->hash = hashcode;
    entry->next = hm->table[i];
    hm->table[i] = entry;
    
    hm->size++;
    if(hm->size >= hm->threshold)
        resize(hm, 2*hm->capacity);
}

static void addEntry_string(acc_hashmap* hm, const char* string_key, param_t* value, int hashcode, int i)
{
    acc_hm_entry* entry = (acc_hm_entry*)malloc(sizeof(acc_hm_entry));
    
    entry->string_key = string_key;
    entry->value = value;    
    entry->hash = hashcode;
    entry->next = hm->table[i];
    hm->table[i] = entry;
    
    hm->size++;
    if(hm->size >= hm->threshold)
        resize(hm, 2*hm->capacity);
}

static void resize(acc_hashmap* hm, int new_capacity)
{
    acc_hm_entry** new_table;
    int old_capactiy = hm->capacity;
    int i, index;    

    if(old_capactiy == HM_MAXIMUM_CAPACITY)
    {
        hm->threshold = INT_MAX;
        return; 
    }

    new_table = (acc_hm_entry**)malloc(new_capacity*sizeof(acc_hm_entry*));
    memset(new_table, 0, new_capacity*sizeof(acc_hm_entry*));
   
    /* iterate over all buckets */ 
    for(i = 0; i < old_capactiy; i++)
    {
        acc_hm_entry* e = hm->table[i];
        if(e != NULL)
        {
            /* iterate over all entries in one bucket */
            do{
                acc_hm_entry* next = e->next;
                index = indexFor(e->hash, new_capacity);
                e->next = new_table[index];
                new_table[index] = e;
                e = next;  
            }while(e != NULL);
        }
    }

    free(hm->table);
    hm->table = new_table;
    hm->capacity = new_capacity;
    hm->threshold = (int)(new_capacity*hm->load_factor);
}


/* insert a key-map pair entry into the hashmap */
param_t* acc_hashmap_put_string(void* _hm, const char* string_key, param_t* value)
{
    acc_hashmap* hm = (acc_hashmap*)_hm; 
    int hashcode;

    hashcode = hash_string(string_key);
    int i = indexFor(hashcode, hm->capacity);

    acc_hm_entry* e;
    
    for(e = hm->table[i]; e != NULL; e = e->next)
    {
        /* if the entry is already in the map */
        if((e->hash == hashcode) && (strcmp(e->string_key, string_key) == 0))
        {
            /* get the old value */
            param_t *old_value = e->value;
            /* update the old value with the new value */
            e->value = value;
            /* return the old value */
            return old_value;
        }
    }

    /* if the entry is a new entry, then add to the hashmap */
    addEntry_string(hm, string_key, value, hashcode, i);

    return NULL;
}

/* insert a key-map pair entry into the hashmap */
param_t* acc_hashmap_put(void* _hm, int key, param_t* value)
{
    acc_hashmap* hm = (acc_hashmap*)_hm; 
    int hashcode;

    hashcode = hash(key);
    int i = indexFor(hashcode, hm->capacity);

    acc_hm_entry* e;
    
    for(e = hm->table[i]; e != NULL; e = e->next)
    {
        /* if the entry is already in the map */
        if((e->hash == hashcode) && (e->key == key))
        {
            /* get the old value */
            param_t *old_value = e->value;
            /* update the old value with the new value */
            e->value = value;
            /* return the old value */
            return old_value;
        }
    }

    /* if the entry is a new entry, then add to the hashmap */
    addEntry(hm, key, value, hashcode, i);

    return NULL;
}

/* get a key-value entry from the hashmap based on the key value */
param_t* acc_hashmap_get_string(void* _hm, const char* string_key)
{
    acc_hashmap* hm = (acc_hashmap*)_hm;
    int hashcode;
    acc_hm_entry* e;

    hashcode = hash_string(string_key);

    int i = indexFor(hashcode, hm->capacity);
    
    for(e = hm->table[i]; e !=NULL; e = e->next)
    {
       if((e->hash == hashcode) && (strcmp(e->string_key, string_key) == 0))
        return e->value; 
    }

    return NULL;
}

/* get the key value based on the key */
param_t* acc_hashmap_get(void* _hm, int key)
{
    acc_hashmap* hm = (acc_hashmap*)_hm;
    int hashcode;
    acc_hm_entry* e;

    hashcode = hash(key);

    int i = indexFor(hashcode, hm->capacity);
    
    for(e = hm->table[i]; e !=NULL; e = e->next)
    {
       if((e->hash == hashcode) && (e->key == key))
        return e->value; 
    }

    return NULL;
}

/* get the key value based on the index */
param_t* acc_hashmap_get_from_index(void* _hm, int index, void* pDevice)
{
    acc_hashmap* hm = (acc_hashmap*)_hm;
    acc_hm_entry* e;

    for(e = hm->table[index]; e !=NULL; e = e->next)
    {
       if(e->value->device_addr == pDevice)
        return e->value; 
    }

    return NULL;
}


/* remove a key-value entry from the hashmap based on the key value */
void* acc_hashmap_remove(void* _hm, int key)
{
    acc_hashmap* hm = (acc_hashmap*)_hm;
    int hashcode;
    acc_hm_entry* prev;
    acc_hm_entry* e;

    hashcode = hash(key);
    int i = indexFor(hashcode, hm->capacity);
    
    prev = hm->table[i];
    e = prev;

    while(e != NULL)
    {
        acc_hm_entry* next = e->next;
        if((e->hash == hashcode) && (e->key == key))
        {
            hm->size-- ;
            if(prev == e)
                hm->table[i] = next;
            else
                prev->next = next;
            break;
        }    

        prev = e;
        e = next;
    }

    if(e != NULL)
    {
        param_t* value = e->value;
        free(e);
        return value;
    }else
        return NULL;
}


/* remove all key-value mappings in this hashmap */
void acc_hashmap_clear(void* _hm)
{
    acc_hashmap* hm = (acc_hashmap*)_hm;
    acc_hm_entry* e;
    acc_hm_entry* item;
    int capacity;
    int i;

    capacity = hm->capacity;
    
    for(i = 0; i < capacity; i++) 
    {
        e = hm->table[i];
        while(e != NULL)
        {
            item = e;
            e = e->next;
            free(item);
        }   
        hm->table[i] = NULL;
    }
    hm->size = 0;
}

/* destroy the hashmap */
void acc_hashmap_destroy(void* _hm)
{
    acc_hashmap* hm = (acc_hashmap*)_hm;
    acc_hm_entry* e;
    acc_hm_entry* item;
    int capacity;
    int i;

    capacity = hm->capacity;
    
    for(i = 0; i < capacity; i++)
    {
        e = hm->table[i]; 
        while(e!= NULL)
        {
            item = e;
            e = e->next;
            free(item);
        }
    }
   
    hm->capacity = 0;
    hm->size = 0;
    free(hm->table);
    free(hm); 
} 
