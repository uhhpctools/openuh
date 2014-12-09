#include <math.h>
#include <stdlib.h>
#include <strings.h>
#include <alloca.h>
#include <limits.h>

#include "team.h"
#include "caf_rtl.h"
#include "comm.h"
#include "alloc.h"
#include "uthash.h"
#include "utlist.h"
#include "profile.h"
#include "trace.h"
#include "util.h"

extern unsigned long _this_image;
extern unsigned long _num_images;
extern unsigned long _log2_images;
extern unsigned long _rem_images;

extern int total_num_supernodes;

extern team_barrier_t team_barrier_algorithm;

team_type initial_team;

team_stack_t *global_team_stack;        /*Initialized in comm_init() */
static alloc_dp_slot *tmp_allocated_list = NULL;

extern void *get_remote_address(void *src, size_t proc);

/* allocated in comm_init*/
team_info_t *exchange_teaminfo_buf;
enum exchange_algorithm alltoall_exchange_algorithm = ALLTOALL_BRUCK;

static void push_stack(team_type_t * team);
static team_type_t *pop_stack();
static team_type_t *stack_top();

void __change_to(team_type team);

void __compute_log2(unsigned long nums_img, unsigned long *log2_nums,
                    unsigned long *rem_nums);

void __alltoall_exchange(team_info_t * my_tinfo, ssize_t len_t_info,
                         team_info_t * team_info_list,
                         const team_type current_team);

int __alltoall_exchange_primi(team_info_t * my_tinfo, ssize_t len_t_info,
                              team_info_t * team_info_list,
                              const team_type current_team);

int __alltoall_exchange_bruck(team_info_t * my_tinfo, ssize_t len_t_info,
                              team_info_t * team_info_list,
                              const team_type current_team);

int __alltoall_exchange_log2polling(team_info_t * my_tinfo,
                                    ssize_t len_t_info,
                                    team_info_t * team_info_list,
                                    const team_type current_team);

void __setup_subteams(team_type new_team_p, team_info_t * team_info_t,
                      int item_counts, int my_teamid);

void __place_codimension_mapping(team_info_t * team_info_list,
                                 team_type new_team_p);

int cmp_team(team_type team1, team_type team2);

void _FORM_TEAM(int *team_id, team_type * new_team_p, int *new_index,
                int *status, int stat_len, char *errmsg, int errmsg_len)
{
    unsigned long my_rank, numimages;
    int i;
    team_info_t my_tinfo;
    team_type new_team;

    LIBCAF_TRACE(LIBCAF_LOG_TEAM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_TEAM);

    my_rank = current_team->current_this_image - 1;
    numimages = current_team->current_num_images;

    if (*team_id <= 0) {
        Error("TEAM_ID argument must be positive for FORM TEAM statement");
    }

    if (new_team_p == NULL) {
        new_team_p = (team_type *) malloc(sizeof(team_type));
        *new_team_p = (team_type) malloc(sizeof(team_type_t));
    } else if (*new_team_p == NULL) {
        *new_team_p = (team_type) malloc(sizeof(team_type_t));
    } else {
        if ((*new_team_p)->leader_set != NULL) {
            free((*new_team_p)->leader_set);
        }
        if ((*new_team_p)->intranode_set != NULL) {
            free((*new_team_p)->intranode_set);
        }
        if ((*new_team_p)->intranode_barflags != NULL) {
            free((*new_team_p)->intranode_barflags);
        }

        /* there is a potential memory leak here. we do not know if this has
         * been implicitly deallocated or not ... so we assume it was or will
         * be, and do NOT deallocate here. */

        /*
           if ((*new_team_p)->barrier.bstep != NULL) {
           coarray_deallocate_((*new_team_p)->barrier.bstep, NULL);
           }
         */
    }

    memset(*new_team_p, 0, sizeof(team_type_t));

    new_team = *new_team_p;

    my_tinfo.team_id = *team_id;
    my_tinfo.index = (new_index != NULL) ? *new_index : 0;

    __alltoall_exchange(&(my_tinfo), sizeof(my_tinfo),
                        exchange_teaminfo_buf, current_team);

    __setup_subteams(new_team, exchange_teaminfo_buf, numimages, *team_id);

    __place_codimension_mapping(exchange_teaminfo_buf, new_team);

    new_team->defined = 1;
    new_team->activated = 0;
    new_team->depth = current_team->depth + 1;
    new_team->parent = current_team;
    new_team->intranode_barflags = malloc(new_team->intranode_set[0] *
                                          sizeof(*new_team->
                                                 intranode_barflags));
    new_team->barrier.parity = 0;
    new_team->barrier.sense = 0;
    new_team->barrier.bstep = NULL;

    /* compute log2_images and rem_images */
    {
        int log2_procs = 0;
        long n = new_team->current_num_images;
        long m = 1;
        while (n > 0) {
            static int first = 1;
            if (first) {
                first = 0;
            } else {
                log2_procs++;
                m <<= 1;
            }
            n >>= 1;
        }
        long rem_procs = new_team->current_num_images - m;
        new_team->current_log2_images = log2_procs;
        new_team->current_rem_images = rem_procs;
    }

    /* compute maximum subteam size */
    int sz = new_team->current_num_images;
    hashed_cdmapping_t *subteam;
    for (subteam = new_team->sibling_list; subteam != NULL;
         subteam = subteam->hh.next) {
        if (subteam->num_images > sz)
            sz = subteam->num_images;
    }

    new_team->intranode_barflags[0] =
        (barrier_flags_t *)
        coarray_allocatable_allocate_(sizeof(*new_team-> intranode_barflags[0]),
                                      NULL, NULL);
    int num_nonleaders = new_team->intranode_set[0] - 1;
    memset(&new_team->intranode_barflags[1], 0,
           num_nonleaders * sizeof(*new_team->intranode_barflags));

    int num_steps = (int) ceil(log2((double) sz));
    new_team->barrier.bstep =
        (barrier_round_t *)
        coarray_allocatable_allocate_(sizeof(barrier_round_t) *
                                      num_steps, NULL, NULL);
    memset(new_team->barrier.bstep, 0,
           sizeof(barrier_round_t) * num_steps);


    /* precompute barrier information */
    {
        int i;
        int ofst = 1;
        int rank = new_team->current_this_image - 1;
        int my_proc = new_team->codimension_mapping[rank];
        long leader = new_team->intranode_set[1];
        long *leader_set = new_team->leader_set;
        int leaders_count = new_team->leaders_count;
        int intranode_count = new_team->intranode_set[0];

        if (team_barrier_algorithm == BAR_2LEVEL_MULTIFLAG ||
            team_barrier_algorithm == BAR_2LEVEL_SHAREDCOUNTER) {
            /* node-aware barrier, so only leaders participate in
             * dissemination barrier */
            int nums = new_team->current_num_images;

            if (my_proc == leader) {
                int num_flags =
                    1 + (int) ceil(log2((double) leaders_count));
                int leader_rank = 0;

                for (i = 1; i < intranode_count; i++) {
                    int target = new_team->intranode_set[i + 1];
                    new_team->intranode_barflags[i] =
                        comm_get_sharedptr(new_team->intranode_barflags[0],
                                           target);
                }

                for (i = 0; i < leaders_count; i++) {
                    if (leader_set[i] == my_proc) {
                        leader_rank = i;
                        break;
                    }
                }

                for (i = 0; i < num_flags; i++) {
                    int target =
                        leader_set[(leader_rank + ofst) % leaders_count];
                    int source =
                        leader_set[(leader_rank - ofst +
                                    leaders_count) % leaders_count];
                    new_team->barrier.bstep[i].target = target;
                    new_team->barrier.bstep[i].source = source;
                    new_team->barrier.bstep[i].remote = (barrier_flags_t *)
                        get_remote_address(&new_team->barrier.bstep[i].
                                           local, target);
                    ofst = ofst * 2;
                }

            } else {
                int leader = new_team->intranode_set[1];
                new_team->intranode_barflags[1] =
                    comm_get_sharedptr(new_team->intranode_barflags[0],
                                       leader);
            }
        } else {
            /* barrier is not node aware, so everyone participates in
             * dissemination barrier */
            int nums = new_team->current_num_images;
            int num_flags = new_team->current_log2_images;
            if (new_team->current_rem_images != 0)
                num_flags += 1;
            for (i = 0; i < num_flags; i++) {
                int target =
                    (new_team->codimension_mapping)[(rank + ofst) % nums];
                int source =
                    (new_team->codimension_mapping)[(rank - ofst + nums) %
                                                    nums];
                new_team->barrier.bstep[i].target = target;
                new_team->barrier.bstep[i].source = source;
                new_team->barrier.bstep[i].remote = (barrier_flags_t *)
                    get_remote_address(&new_team->barrier.bstep[i].local,
                                       target);
                ofst = ofst * 2;
            }
        }
    }

    new_team->symm_mem_slot.start_addr = NULL;
    new_team->symm_mem_slot.end_addr = NULL;

    comm_sync_all(status, stat_len, errmsg, errmsg_len);

    PROFILE_FUNC_EXIT(CAFPROF_TEAM);
    LIBCAF_TRACE(LIBCAF_LOG_TEAM, "exit");
}

void _CHANGE_TEAM(team_type * new_team_p,
                  int *status, int stat_len, char *errmsg, int errmsg_len)
{
    team_type new_team;
    extern unsigned long _this_image;
    extern unsigned long _num_images;
    extern shared_memory_slot_t *init_common_slot;
    extern shared_memory_slot_t *child_common_slot;
    LIBCAF_TRACE(LIBCAF_LOG_TEAM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_TEAM);

    new_team = *new_team_p;
    if (new_team == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "changing team not specified");
    }

    /*If change to new child team, get the addr of child_common_slot as start addr
     * and the end addr = start_addr+size
     * for the current_team, if it is not initial team, put the end addr =
     * child_common_slot->addr.
     *
     * If change to initial team from any child team, I will record the address of
     * intial_common_slot to a tmp_address, for use in end_change_team
     */
    if (new_team->depth != 0) { //change to child team
        new_team->symm_mem_slot.start_addr = child_common_slot->addr;
        new_team->symm_mem_slot.end_addr =
            child_common_slot->addr + child_common_slot->size;
        if (current_team->depth != 0) {
            current_team->symm_mem_slot.end_addr = child_common_slot->addr;
        }
    } else {                    //swap(current_hash, tmp_hash)
        tmp_allocated_list = new_team->allocated_list;
        new_team->allocated_list = NULL;
    }

    __change_to(new_team);

    push_stack(new_team);

    comm_sync_all(status, stat_len, errmsg, errmsg_len);

    PROFILE_FUNC_EXIT(CAFPROF_TEAM);
    LIBCAF_TRACE(LIBCAF_LOG_TEAM, "exit");
}

void _END_TEAM(int *status, int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_TEAM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_TEAM);

    //move back to its parent, cleanup WITH CARE!
    //do something in case someone is communication with me,barrier or pending something?
    __coarray_wait_all();

    team_type tmp_cur_team, tmp_team;
    hashed_cdmapping_t *current_node, *tmp_node;
    tmp_cur_team = pop_stack();

    deallocate_team_all();
    tmp_team = stack_top();

    comm_sync_all(status, stat_len, errmsg, errmsg_len);
    if (current_team->depth == 0)       //if current_team is initial team,restore the hash table
    {
        current_team->allocated_list = tmp_allocated_list;
    }

    __change_to(tmp_team);      //move out

    PROFILE_FUNC_EXIT(CAFPROF_TEAM);
    LIBCAF_TRACE(LIBCAF_LOG_TEAM, "exit");
}

void clear_team_()
{

}

void __change_to(team_type_t * team)
{
    _this_image = team->current_this_image;
    _num_images = team->current_num_images;
    _log2_images = team->current_log2_images;
    _rem_images = team->current_rem_images;
    current_team = team;
    current_team->activated = 1;
}

int cmp_team(team_type team1, team_type team2)
{
    return (team1->team_id == team2->team_id);
}

void __compute_log2(unsigned long nums_img, unsigned long *log2_nums,
                    unsigned long *rem_nums)
{
    int log2_procs = 0;
    long m = 1;
    long n = nums_img;
    while (n > 0) {
        static int first = 1;
        if (first) {
            first = 0;
        } else {
            *log2_nums++;
            m <<= 1;
        }
        n >>= 1;
    }

    *rem_nums = nums_img - m;
}


void __alltoall_exchange(team_info_t * my_tinfo, ssize_t len_t_info,
                         team_info_t * exchange_info_buffer,
                         const team_type current_team)
{
    int retval;
    long num_images = current_team->current_num_images;

    memset(exchange_info_buffer, 0, sizeof(team_info_t) * num_images);
    switch (alltoall_exchange_algorithm) {
    case ALLTOALL_PRIMI:
        retval =
            __alltoall_exchange_primi(my_tinfo, len_t_info,
                                      exchange_teaminfo_buf, current_team);
        break;
    case ALLTOALL_LOG2POLLING:
        retval =
            __alltoall_exchange_log2polling(my_tinfo, len_t_info,
                                            exchange_teaminfo_buf,
                                            current_team);
        break;
    case ALLTOALL_BRUCK:
        retval =
            __alltoall_exchange_bruck(my_tinfo, len_t_info,
                                      exchange_teaminfo_buf, current_team);
        break;
    default:
        retval =
            __alltoall_exchange_bruck(my_tinfo, len_t_info,
                                      exchange_teaminfo_buf, current_team);
    }
    /*error handling */
}

int __alltoall_exchange_primi(team_info_t * my_tinfo, ssize_t len_t_info,
                              team_info_t * team_info_list,
                              const team_type current_team)
{
    long this_rank, numimages;
    int i, errstatus;
    char errmsg[128];

    this_rank = current_team->current_this_image - 1;
    numimages = current_team->current_num_images;

    for (i = 1; i <= numimages; i++) {
        __coarray_write(i, &(team_info_list[this_rank]), my_tinfo,
                        sizeof(team_info_t), 1, NULL);
    }
    comm_sync_all(&(errstatus), sizeof(int), errmsg, 128);
}

int __alltoall_exchange_bruck(team_info_t * my_tinfo, ssize_t len_t_info,
                              team_info_t * team_info_list,
                              const team_type current_team)
{
    long this_rank, numimages;
    long send_peer, recv_peer;  //send_peer refer to the image "I" am going to send
    int round, max_round;
    int offset, rem_slots, num_data;
    int reorder_srtidx, cpysize;
    int *flag_coarray;
    team_info_t *reorder_buffer;
    int errstatus;
    char errmsg[128];


    this_rank = current_team->current_this_image - 1;
    numimages = current_team->current_num_images;

    reorder_buffer =
        (team_info_t *) malloc(sizeof(team_info_t) * numimages);
    memset(reorder_buffer, 0, sizeof(team_info_t) * numimages);

    max_round = ceil(log2((double) numimages));

    /*Flag_coarray indicate if recv_peer have sent data to me */
    flag_coarray =
        (int *) coarray_allocatable_allocate_(sizeof(int) * max_round,
                                              NULL, NULL);
    memset(flag_coarray, 0, sizeof(int) * max_round);
    comm_sync_all(NULL, 0, NULL, 0);

    /*step 1, initial data */
    team_info_list[0].team_id = my_tinfo->team_id;
    team_info_list[0].index = my_tinfo->index;

    rem_slots = numimages - 1;

    /*step 2, exchange */
    for (round = 0, offset = 0; round < max_round; round++) {
        //in each round, image i send team_info of number min{rem_slots, offset}
        // to (i-offset) and waiting for the (i+offset)
        if (rem_slots == 0)
            break;
        offset = pow(2, round);
        num_data = offset <= rem_slots ? offset : rem_slots;
        send_peer = (this_rank - offset + numimages) % numimages;
        recv_peer = (this_rank + offset) % numimages;
        /*Send to my send_peer */
        //need tracing here?
        comm_write(current_team->codimension_mapping[send_peer],
                   &(team_info_list[numimages - rem_slots]),
                   team_info_list, sizeof(team_info_t) * num_data,
                   1, NULL);
        comm_write((current_team->codimension_mapping[send_peer]),
                   &(flag_coarray[round]), &(num_data),
                   sizeof(int), 1, NULL);

        /*Wait on my recv_peer */
        rem_slots -= num_data;
        while (!SYNC_SWAP(&flag_coarray[round], 0));
    }

    /*step 3, local reorder */
    if (this_rank != 0) {
        reorder_srtidx = numimages - this_rank;
        cpysize = this_rank;
        memcpy(reorder_buffer, &(team_info_list[reorder_srtidx]),
               cpysize * sizeof(team_info_t));
        memcpy(&(reorder_buffer[cpysize]), team_info_list,
               reorder_srtidx * sizeof(team_info_t));
        memcpy(team_info_list, reorder_buffer,
               numimages * sizeof(team_info_t));
    }

    /*Clean flag_coarray */
    coarray_deallocate_(flag_coarray, NULL);
    free(reorder_buffer);
}

int __alltoall_exchange_log2polling(team_info_t * my_tinfo,
                                    ssize_t len_t_info,
                                    team_info_t * team_info_list,
                                    const team_type current_team)
{
    long this_rank, numimages;
    int i, errstatus;
    char errmsg[128];
    /*TODO: */
}

void __setup_subteams(team_type new_team, team_info_t * team_info_t,
                      int item_counts, int my_teamid)
{
    int i;
    typedef struct hashed_metadata_node {
        unsigned int id;
        unsigned int count;
        UT_hash_handle hh;
    } hashed_metadata_node;

    hashed_metadata_node *hashed_md_list = NULL;

    for (i = 0; i < item_counts; i++) {
        hashed_metadata_node *exist_node;
        HASH_FIND_INT(hashed_md_list, &(team_info_t[i].team_id),
                      exist_node);
        if (exist_node != NULL) {
            exist_node->count += 1;
        } else {
            hashed_metadata_node *new_node;
            new_node = (hashed_metadata_node *)
                malloc(sizeof(hashed_metadata_node));
            new_node->id = team_info_t[i].team_id;
            new_node->count = 1;
            HASH_ADD_INT(hashed_md_list, id, new_node);
        }
    }

    new_team->sibling_list = NULL;

    hashed_metadata_node *current_node;
    for (current_node = hashed_md_list; current_node != NULL;
         current_node = current_node->hh.next) {
        if (current_node->id != my_teamid)      //not in my team
        {
            int count = current_node->count;
            hashed_cdmapping_t *sibling_mp_node;
            HASH_FIND_INT(new_team->sibling_list, &(current_node->id),
                          sibling_mp_node);
            if (sibling_mp_node == NULL) {
                sibling_mp_node = (hashed_cdmapping_t *)
                    malloc(sizeof(hashed_cdmapping_t));
                sibling_mp_node->team_id = current_node->id;
                sibling_mp_node->num_images = count;
                sibling_mp_node->codimension_mapping =
                    (long *) malloc(sizeof(long) * count);
                memset((sibling_mp_node->codimension_mapping), -1,
                       sizeof(long) * count);
                HASH_ADD_INT(new_team->sibling_list, team_id,
                             sibling_mp_node);
            }
        } else                  // in my team
        {
            int count = current_node->count;
            new_team->team_id = my_teamid;
            new_team->current_num_images = count;       //current_team->current_num_images;
            new_team->codimension_mapping =
                (long *) malloc(sizeof(long) * (count));
            memset((new_team->codimension_mapping), -1,
                   sizeof(long) * count);
        }

    }

    //clean up the metadata list
    hashed_metadata_node *current_md_node, *temp_node;
    HASH_ITER(hh, hashed_md_list, current_md_node, temp_node) {
        HASH_DEL(hashed_md_list, current_md_node);
        free(current_md_node);
    }

}

void __place_codimension_mapping(team_info_t * team_info_list,
                                 team_type new_team_p)
{
    /*
     **  Two pass
     **  1. insert those who asserts a new index
     **  2. insert the rest in order
     */
    int my_rank, numimages;
    int i, j;
    long *p_mapping;

    hashed_cdmapping_t *sibling_team_node;

    my_rank = current_team->current_this_image - 1;
    numimages = current_team->current_num_images;
    /*First Pass */
    for (int i = 0; i < numimages; ++i) {
        if (team_info_list[i].index != 0) {
            if (team_info_list[i].team_id == new_team_p->team_id) {
                p_mapping = new_team_p->codimension_mapping;
                int tmp_index = team_info_list[i].index;
                if (tmp_index < 0
                    || tmp_index > new_team_p->current_num_images) {
                    Error("1a. tmp_index is not correct! (tmp_index = %d)",
                          tmp_index);
                    //error handle
                }
                if (p_mapping[tmp_index - 1] != -1) {
                    Error("1b. p_mapping is not initialized to -1");
                    //error handle
                }
                p_mapping[tmp_index - 1] =
                    *(current_team->codimension_mapping + i);
            } else              // in my sibling team
            {
                HASH_FIND_INT(new_team_p->sibling_list,
                              &(team_info_list[i].team_id),
                              sibling_team_node);
                p_mapping = sibling_team_node->codimension_mapping;
                int tmp_index = team_info_list[i].index;
                if (tmp_index < 0)      //|| tmp_index > sibling_team_node->count)
                {
                    //error handle
                    Error("2a. tmp_index is not correct! (tmp_index = %d)",
                          tmp_index);
                }
                if (p_mapping[tmp_index - 1] != -1) {
                    //error handle
                    Error("2b. p_mapping is not initialized to -1");
                }
                p_mapping[tmp_index - 1] =
                    *(current_team->codimension_mapping + i);
            }
        }
    }
    int k = 0;
    /* Second Pass */
    for (int i = 0; i < numimages; ++i) {
        if (team_info_list[i].index == 0) {
            if (team_info_list[i].team_id == new_team_p->team_id)       // in my team
            {
                p_mapping = new_team_p->codimension_mapping;
                k = 0;
                while (p_mapping[k] != -1)
                    k++;
                p_mapping[k] = *(current_team->codimension_mapping + i);
            } else              //in sibling team
            {
                HASH_FIND_INT(new_team_p->sibling_list,
                              &(team_info_list[i].team_id),
                              sibling_team_node);
                p_mapping = sibling_team_node->codimension_mapping;
                k = 0;
                while (p_mapping[k] != -1)
                    k++;
                p_mapping[k] = *(current_team->codimension_mapping + i);
            }
        }
        /* code */
    }

    for (k = 0; k < new_team_p->current_num_images; k++) {
        if (new_team_p->codimension_mapping[k] ==
            current_team->codimension_mapping[my_rank]) {
            new_team_p->current_this_image = k + 1;
            break;
        }
    }

    /* Form new supernode team */
    {
        long *tmp_intranode_set = (long *) malloc(sizeof(long) *
                                                  (1 +
                                                   current_team->
                                                   intranode_set[0]));
        long count = 0;
        long current_proc;
        for (i = 0; i < new_team_p->current_num_images; i++) {
            current_proc = new_team_p->codimension_mapping[i];
            for (j = 1; j <= current_team->intranode_set[0]; j++) {
                if (current_proc == current_team->intranode_set[j]) {
                    tmp_intranode_set[++count] = current_proc;
                    break;
                }
            }
        }
        tmp_intranode_set[0] = count;
        new_team_p->intranode_set = (long *) malloc(sizeof(long) *
                                                    (count + 1));
        memcpy(new_team_p->intranode_set, tmp_intranode_set,
               sizeof(long) * (count + 1));
        free(tmp_intranode_set);
    }
    /*Form new leader set */
    {
        long *tmp_leader_set = (long *) malloc(sizeof(long) *
                                               total_num_supernodes);
        int k = 0;
        for (i = 0; i < total_num_supernodes; i++)
            tmp_leader_set[i] = LONG_MAX;
        int count = 0;
        int spnode_id = 0;
        long tmp_proc_id = 0;
        for (i = 0; i < new_team_p->current_num_images; i++) {
            tmp_proc_id = new_team_p->codimension_mapping[i];
            spnode_id = comm_get_node_id(tmp_proc_id);
            if (tmp_leader_set[spnode_id] > i) {
                tmp_leader_set[spnode_id] = i;
                count++;
            }
        }

        new_team_p->leader_set = malloc(sizeof(long) * (count));
        new_team_p->leaders_count = count;
        for (i = 0; i < total_num_supernodes; i++) {
            if (tmp_leader_set[i] < LONG_MAX) { //has leader for this node
                new_team_p->leader_set[k++] =
                    new_team_p->codimension_mapping[tmp_leader_set[i]];
                if (k > count)
                    Warning("Mismatch leader count");
            }
        }

        free(tmp_leader_set);
    }
}

int team_id__(team_type *team_p)
{
    if (team_p == NULL)
        return current_team->team_id;
    else if (*team_p) {
        return (*team_p)->team_id;
    } else {
        Error("Invalid TEAM argument for TEAM_ID");
    }
}

team_type get_team__(enum PREDEF_TEAM_LEVELS *team_level)
{
    if (team_level == NULL) {
        return current_team;
    }

    switch (*team_level) {
    case INITIAL_TEAM:
        return initial_team;
    case PARENT_TEAM:
        return current_team->parent;
    case CURRENT_TEAM:
        return current_team;
    default:
        Error("Encountered unknown team level %d", (int) *team_level);
        /* doesn't reach */
    }

    return 0;
}

long image_to_procid(long image, team_type team)
{
    return team->codimension_mapping[image - 1];
}

/*Push the team pointer into the stack*/
void push_stack(team_type_t * team_ptr)
{
    if (global_team_stack->count >= MAX_NUM_TEAM)
        Error("TEAM TREE HAS BEEN TOO DEEP");
    team_type_t *cur_top = stack_top();

    global_team_stack->stack[global_team_stack->count] = team_ptr;
    global_team_stack->count += 1;
}

/*Pop out the latest team pointer and return it*/
team_type_t *pop_stack()
{
    if (global_team_stack->count < 1)
        Error("NO TEAMS IN THE STACK");
    team_type_t *retval =
        global_team_stack->stack[global_team_stack->count - 1];
    global_team_stack->stack[global_team_stack->count - 1] = NULL;
    global_team_stack->count -= 1;
    return retval;
}

team_type_t *stack_top()
{
    if (global_team_stack->count == 0)
        Error("NO TEAMS IN THE STACK");
    team_type_t *retval =
        global_team_stack->stack[global_team_stack->count - 1];
    return retval;
}
