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
    long *codimension_mapping;
    UT_hash_handle hh;
} hashed_cdmapping_t;

typedef struct team {
    long *codimension_mapping;
    int defined;
    int activated;
    int team_id;
    unsigned long current_this_image;
    unsigned long current_num_images;
    unsigned long current_log2_images;  //log2_procs
    unsigned long current_rem_images;   //rem_procs
    int depth;
    struct team *parent;
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
