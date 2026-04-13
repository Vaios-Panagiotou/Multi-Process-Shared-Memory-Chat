#include "dialog.h"
#include <semaphore.h>
#include <stdio.h>

/*
 ftiaxnei neo dialogo kai bazei mesa tin diergasia.
 xrisimopoiei to mutex gia na min ginei mperdema me tin mnimi.
 */
int create_dialog(shm_region_t *shared_mem,pid_t process_id) {
    int new_dialog_id;

    sem_wait(&shared_mem->mutex);
    
    new_dialog_id=create_dialog_and_join_locked(shared_mem,process_id);
    
    sem_post(&shared_mem->mutex);

    return new_dialog_id;
}

/*
 prosthetei mia diergasia se enan iparxon dialogo.
 epistrefei 0 an ola pane kala, alliws kodiko lathous.
 */
int join_dialog(shm_region_t *shared_mem,int dlg_id,pid_t process_id) {
    int status;

    sem_wait(&shared_mem->mutex);
    status = join_dialog_locked(shared_mem,dlg_id,process_id);
    sem_post(&shared_mem->mutex);

    return status;
}

/*
 afairei mia diergasia apo enan dialogo.
 */
int leave_dialog(shm_region_t *shared_mem,int dlg_id,pid_t process_id) {
    int result;

    sem_wait(&shared_mem->mutex);
    result = leave_dialog_locked(shared_mem,dlg_id,process_id);
    sem_post(&shared_mem->mutex);

    return result;
}