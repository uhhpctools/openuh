

char OMP_EVENT_NAME[22][50]= {
        "OMP_EVENT_FORK",
        "OMP_EVENT_JOIN",
        "OMP_EVENT_THR_BEGIN_IDLE",
        "OMP_EVENT_THR_END_IDLE",
        "OMP_EVENT_THR_BEGIN_IBAR",
        "OMP_EVENT_THR_END_IBAR",
        "OMP_EVENT_THR_BEGIN_EBAR",
        "OMP_EVENT_THR_END_EBAR",
        "OMP_EVENT_THR_BEGIN_LKWT",
        "OMP_EVENT_THR_END_LKWT",
        "OMP_EVENT_THR_BEGIN_CTWT",
        "OMP_EVENT_THR_END_CTWT",
        "OMP_EVENT_THR_BEGIN_ODWT",
        "OMP_EVENT_THR_END_ODWT",
        "OMP_EVENT_THR_BEGIN_MASTER",
        "OMP_EVENT_THR_END_MASTER",
        "OMP_EVENT_THR_BEGIN_SINGLE",
        "OMP_EVENT_THR_END_SINGLE",
        "OMP_EVENT_THR_BEGIN_ORDERED",
        "OMP_EVENT_THR_END_ORDERED",
        "OMP_EVENT_THR_BEGIN_ATWT",
        "OMP_EVENT_THR_END_ATWT" };


char OMP_STATE_NAME[11][50]= {
    "THR_OVHD_STATE",          /* Overhead */
    "THR_WORK_STATE",          /* Useful work, excluding reduction, master, single, critical */
    "THR_IBAR_STATE",          /* In an implicit barrier */
    "THR_EBAR_STATE",          /* In an explicit barrier */
    "THR_IDLE_STATE",          /* Slave waiting for work */
    "THR_SERIAL_STATE",        /* thread not in any OMP parallel region (initial thread only) */
    "THR_REDUC_STATE",         /* Reduction */
    "THR_LKWT_STATE",          /* Waiting for lock */
    "THR_CTWT_STATE",          /* Waiting to enter critical region */
    "THR_ODWT_STATE",          /* Waiting to execute an ordered region */
    "THR_ATWT_STATE"};         /* Waiting to enter an atomic region */




