#ifndef TEAM_H
#define TEAM_H
#include <ctype.h>
#include "alloc.h"
#include "uthash.h"

#define MAX_NUM_TEAM 256

enum PREDEF_TEAM_LEVELS {
    INITIAL_TEAM = 201,
    PARENT_TEAM = 202,
    CURRENT_TEAM = 203
};

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
typedef char coll_flags_t;

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
    long *codimension_mapping;
    barrier_flags_t **intranode_barflags;
    barrier_data_t barrier;
    coll_flags_t *coll_syncflags;
    long *intranode_set;
    long *leader_set;
    /* end of first cache line */
    coll_flags_t *allreduce_sync;
    coll_flags_t *reduce_flag;
    coll_flags_t *bcast_flag;
    coll_flags_t *allreduce_flag;
    coll_flags_t *reduce_go;
    coll_flags_t *bcast_go;
    struct team *parent;
    int team_id;
    int leaders_count;
    char defined;
    char activated;
    char allreduce_bufid;
    char reduce_bufid;
    char bcast_bufid;
    int depth;
    unsigned long current_log2_images;  //log2_procs
    unsigned long current_rem_images;   //rem_procs
    hashed_cdmapping_t *sibling_list;
    /*heap address for this team */
    mem_block_t symm_mem_slot;
    alloc_dp_slot *allocated_list;
} team_type_t, *team_type;

typedef struct team_stack_t {
    team_type stack[MAX_NUM_TEAM];
    int count;
} team_stack_t;

typedef enum exchange_algorithm {
    ALLTOALL_NAIVE,
    ALLTOALL_LOG2POLLING,
    ALLTOALL_BRUCK,
    ALLTOALL_BRUCK2
} exchange_algorithm_t;

//global pointer to current team
extern team_type_t *current_team;

extern team_type initial_team;

//resist in shared memory, allocated in comm_init
extern team_info_t *exchange_teaminfo_buf;

/* new_index == -1 specify no particular new index */

void _FORM_TEAM(int *team_id, team_type * new_team_p, int *new_index,
                int *status, int stat_len, char *errmsg, int errmsg_len);

void _CHANGE_TEAM(team_type * new_team_p,
                  int *status, int stat_len, char *errmsg, int errmsg_len);

void _END_TEAM(int *status, int stat_len, char *errmsg, int errmsg_len);

int team_id__(team_type *team_p);

team_type get_team__(enum PREDEF_TEAM_LEVELS *team_level);

#endif                          //TEAM_H
