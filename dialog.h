#ifndef DIALOG_H
#define DIALOG_H


#include "shared.h"


int create_dialog(shm_region_t *shm,pid_t pid);
int join_dialog(shm_region_t *shm,int dialog_id,pid_t pid);
int leave_dialog(shm_region_t *shm,int dialog_id,pid_t pid);


#endif /* DIALOG_H */