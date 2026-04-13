#ifndef MESSAGE_H
#define MESSAGE_H


#include "shared.h"


int send_message(shm_region_t *shm,int dialog_id,pid_t sender,const char *payload);
int collect_messages(shm_region_t *shm,pid_t pid,int *out_idxs,int max_out);


#endif