#include <math.h>
#include <stdlib.h>
#include <strings.h>
#include <alloca.h>
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

team_type initial_team;

/* allocated in comm_init*/
team_info_t *exchange_teaminfo_buf;
enum exchange_algorithm alltoall_exchange_algorithm = ALLTOALL_PRIMI;

void __change_to(team_type team);

void __compute_log2(unsigned long nums_img, unsigned long *log2_nums,
        unsigned long *rem_nums);

void __alltoall_exchange(team_info_t * my_tinfo, ssize_t len_t_info,
        team_info_t * team_info_list,
        const team_type current_team);

int __alltoall_exchange_primi(team_info_t * my_tinfo, ssize_t len_t_info,
        team_info_t * team_info_list,
        const team_type current_team);

int __alltoall_exchange_log2(team_info_t * my_tinfo, ssize_t len_t_info,
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

/*We assume that all images in this team all will call this sync_team*/
void sync_team_(team_type * team_p)
{
    /* sync among all image in *(team_p) using _SNYC_IAMGES,how about using _sync_all */
    int j;
    int *team_images;
    team_type team;
    if (team_p == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "Team specified does not exist");
        return;			/*ERROR HANDLE */
    }
    team = *team_p;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    int *status = (int *) malloc(sizeof(int) * team->current_num_images);

    /* DE: first argument needs to be an array of integers, not long. Also,
     * probably SYNC IMAGES is not the most efficient way to do this.
     */
    team_images = malloc(team->current_num_images * sizeof *team_images);
    for (j = 0; j < team->current_num_images; j++) {
        team_images[j] = (int) team->codimension_mapping[j];
    }
    _SYNC_IMAGES(team_images, team->current_num_images, status,
            team->current_num_images, NULL, 0);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
    /*TODO: Interpret the status and errmsg */
    free(status);
}

team_type *form_team_(int *team_id, team_type * new_team_p, int *new_index)
{
    unsigned long my_rank, numimages;
    int i;
    team_info_t my_tinfo;
    team_type new_team;
    //  team_info_t * team_info_list = NULL; //destination

    my_rank = current_team->current_this_image - 1;
    numimages = current_team->current_num_images;

    if (new_team_p == NULL) {
        new_team_p = (team_type *) malloc(sizeof(team_type));
        *new_team_p = (team_type) malloc(sizeof(team_type_t));
    }

    if (*new_team_p == NULL) {
        *new_team_p = (team_type) malloc(sizeof(team_type_t));
    }
    new_team = *new_team_p;

    my_tinfo.team_id = *team_id;
    my_tinfo.index = (new_index != NULL) ? *new_index : 0;

    __alltoall_exchange(&(my_tinfo), sizeof(my_tinfo),
            exchange_teaminfo_buf, current_team);
    //when sequence reach here, we have correspoding team_info_t in the team_info_list

    __setup_subteams(new_team, exchange_teaminfo_buf, numimages, *team_id);

    __place_codimension_mapping(exchange_teaminfo_buf, new_team);

    new_team->defined = 1;
    new_team->activated = 0;
    new_team->depth = current_team->depth + 1;
    new_team->parent = current_team;
    new_team->symm_mem_slot.start_addr = NULL;
    new_team->symm_mem_slot.end_addr = NULL;

    /*
     ** Maybe need a comm_sync_all?
     */
}

void change_team_(team_type * new_team_p)
{
    team_type new_team;
    extern unsigned long _this_image;
    extern unsigned long _num_images;
    extern shared_memory_slot_t *child_common_slot;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    new_team = *new_team_p;
    if (new_team == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "changing team not specified");
    }
    if (new_team->parent != current_team) {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "Not a child team");
    }

    /*If change to new child team, get the addr of child_common_slot as start addr
     * and the end addr = start_addr+size
     * for the current_team, if it is not team_world, put the end addr =
     * child_common_slot->addr.
     */
    if (new_team->depth != 0) {	//change to child team
        new_team->symm_mem_slot.start_addr = child_common_slot->addr;
        new_team->symm_mem_slot.end_addr =
            child_common_slot->addr + child_common_slot->size;
        if (current_team->depth != 0) {
            current_team->symm_mem_slot.end_addr = child_common_slot->addr;
        }
    }

    __change_to(new_team);

    comm_sync_all(NULL, 0, NULL, 0);
}

void end_change_team_(void)
{
    //move back to its parent, cleanup WITH CARE!
    //do something in case someone is communication with me,barrier or pending something?
    __coarray_wait_all();

    team_type tmp_team;
    hashed_cdmapping_t *current_node, *tmp_node;
    tmp_team = current_team->parent;	//may have error here. what

    deallocate_within(current_team->symm_mem_slot.start_addr,
            current_team->symm_mem_slot.end_addr);
    comm_sync_all(NULL, 0, NULL, 0);
    __change_to(tmp_team);	//move out
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
        case ALLTOALL_LOG2:
            retval =
                __alltoall_exchange_log2(my_tinfo, len_t_info,
                        exchange_teaminfo_buf, current_team);
            break;
        default:
            retval =
                __alltoall_exchange_primi(my_tinfo, len_t_info,
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
    // team_info_list = (team_info_t *)coarray_allocatable_allocate_(sizeof(team_info_t)*numimages,NULL);

    for (i = 1; i <= numimages; i++) {
        __coarray_write(i, &(team_info_list[this_rank]), my_tinfo,
                sizeof(team_info_t), 1, NULL);
    }
    comm_sync_all(&(errstatus), sizeof(int), errmsg, 128);

    // *team_info_list_p = team_info_list;
}

int __alltoall_exchange_log2(team_info_t * my_tinfo, ssize_t len_t_info,
        team_info_t * team_info_list,
        const team_type current_team)
{
    long this_rank, numimages;
    int i, errstatus;
    char errmsg[128];

    this_rank = current_team->current_this_image - 1;
    numimages = current_team->current_num_images;
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
            new_node =
                (hashed_metadata_node *)
                malloc(sizeof(hashed_metadata_node));
            new_node->id = team_info_t[i].team_id;
            new_node->count = 1;
            HASH_ADD_INT(hashed_md_list, id, new_node);
        }
    }
    hashed_metadata_node *current_node;
    for (current_node = hashed_md_list; current_node != NULL;
            current_node = current_node->hh.next) {
        if (current_node->id != my_teamid)	//not in my team
        {
            int count = current_node->count;
            hashed_cdmapping_t *sibling_mp_node;
            HASH_FIND_INT(new_team->sibling_list, &(current_node->id),
                    sibling_mp_node);
            if (sibling_mp_node == NULL) {
                sibling_mp_node =
                    (hashed_cdmapping_t *)
                    malloc(sizeof(hashed_cdmapping_t));
                sibling_mp_node->team_id = current_node->id;
                sibling_mp_node->codimension_mapping =
                    (long *) malloc(sizeof(long) * count);
                memset((sibling_mp_node->codimension_mapping), -1,
                        sizeof(long) * count);
                HASH_ADD_INT(new_team->sibling_list, team_id,
                        sibling_mp_node);
            }
        } else			// in my team
        {
            int count = current_node->count;
            new_team->team_id = my_teamid;
            new_team->current_num_images = count;	//current_team->current_num_images;
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
    int i;
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
            } else		// in my sibling team
            {
                HASH_FIND_INT(new_team_p->sibling_list,
                        &(team_info_list[i].team_id),
                        sibling_team_node);
                p_mapping = sibling_team_node->codimension_mapping;
                int tmp_index = team_info_list[i].index;
                if (tmp_index < 0)	//|| tmp_index > sibling_team_node->count)
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
            if (team_info_list[i].team_id == new_team_p->team_id)	// in my team
            {
                p_mapping = new_team_p->codimension_mapping;
                k = 0;
                while (p_mapping[k] != -1)
                    k++;
                p_mapping[k] = *(current_team->codimension_mapping + i);
            } else		//in sibling team
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
}

int team_id_()
{
    return current_team->team_id;
}
