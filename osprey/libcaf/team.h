#ifndef TEAM_H
#define TEAM_H
#include <ctype.h>
#include "alloc.h"
#include "uthash.h"

typedef struct team_info {
    int team_id;
    int index;
} team_info_t;

typedef struct {
    int team_id;
    int num_images;
    long *codimension_mapping;
    UT_hash_handle hh;
} hashed_cdmapping_t;

typedef char barrier_flags_t;

typedef struct barrier_round {
    barrier_flags_t local[2];
    int target;
    int source;
    barrier_flags_t *remote;
} barrier_round_t;

typedef struct barrier_data {
    short parity;
    short sense;
    barrier_round_t *bstep;
} barrier_data_t;

typedef struct team {
    long current_this_image;
    long current_num_images;
    long * codimension_mapping;
    barrier_flags_t **intranode_barflags;
    barrier_data_t barrier;
    struct team *parent;
    long * intranode_set;
    long * leader_set;
    /* end of first cache line */
    int team_id;
    int leaders_count;
    int defined;
    int activated;
    int depth;
    unsigned long current_log2_images;  //log2_procs
    unsigned long current_rem_images;   //rem_procs
    hashed_cdmapping_t *sibling_list;
    /*heap address for this team */
    mem_block_t symm_mem_slot;
} team_type_t;


typedef team_type_t *team_type;

enum exchange_algorithm { ALLTOALL_PRIMI, ALLTOALL_LOG2POLLING,
    ALLTOALL_BRUCK};

//global pointer to current team
extern team_type_t *current_team;

extern team_type initial_team;

//resist in shared memory, allocated in comm_init
extern team_info_t *exchange_teaminfo_buf;

void sync_team_(team_type * team_p);

/* new_index == -1 specify no particular new index */
team_type *form_team_(int *team_id, team_type * new_team_pp, int *new_index);

void change_team_(team_type * new_team_pp);
void end_change_team_(void);

int team_id_();
team_type get_team_(int* distance_p);

#endif              //TEAM_H
