#include "message.h"

/*στελνει μηνυμα σε συγκεκριμενο διαλογο με συγκεκριμενο αποστολεα,mporo kai xwris const*/
int send_message(shm_region_t *shm,int dialog_id,pid_t sender,const char *payload) 
{
    int ret;
    //κλειδώνω την πρόσβαση στη μοιραζόμενη μνήμη
    sem_wait(&shm->mutex);
    ret = send_message_locked(shm,dialog_id,sender,payload);
    //μετα την ξεκλειδωνω
    sem_post(&shm->mutex);

    return ret;
}

/*wrapper για τη συλλογή μηνυμάτων για ένα συγκεκριμένο PID.*/
int collect_messages(shm_region_t *shm,pid_t pid,int *out_idxs,int max_out) 
{
    //απλώς καλεί την τροποποιημένη συνάρτηση συλλογής μηνυμάτων
    return collect_messages_for_pid(shm,pid,out_idxs,max_out);
}