#include "receiver.h"
#include "message.h"
#include "dialog.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

//φέρνω τις μεταβλητές από τη main.c.
//χρειάζομαι το g_shm για να βλέπω τη μνήμη και το g_running για να ξέρω πότε να σταματήσω.

extern shm_region_t *g_shm;
extern pid_t g_pid;
extern volatile int g_running;
extern pthread_mutex_t g_print_mutex;

void *receiver_thread_fn(void *arg) {//*thread δέκτη που συλλέγει μηνύματα για το τρέχον pid και τα εμφανίζει.*/
    (void)arg;//gia na apofigo to warning pou den xrhsimopoieitai to arg
    int idxs[MAX_MSGS]; 
    //mnimi gia to teleuteo id pou exoume dei
    int local_last_id = 0;
    while (g_running) { 

        /*πρώτα ψάχνω αν υπάρχει δουλειά. 
         *η collect_messages έχει δικό της sem_wait,οπότε δεν χρειάζεται να κλειδώσω κάτι εδώ.
         */
        int got =collect_messages(g_shm,g_pid,idxs,MAX_MSGS);

        /*an vrikame*/
        if (got>0) {
            for (int k= 0;k< got &&g_running;++k) { 
                int mi=idxs[k]; 

                char local_payload[MAX_PAYLOAD]; 
                pid_t sender; 
                int dialog_id; 
                int msg_id;

                sem_wait(&g_shm->mutex);///κλειδώνω για να διαβάσω με ασφάλεια

                int current_msg_id=g_shm->msgs[mi].msg_id;

                //an to minimma to exoume idi dei tto prospername
                if (current_msg_id<=local_last_id) {
                    sem_post(&g_shm->mutex);
                    continue; //πήγαινε στο επόμενο
                }

                //einai neo minimma
                local_last_id=current_msg_id;

                if (g_shm->msgs[mi].remaining_reads> 0) {
                    g_shm->msgs[mi].remaining_reads--; 
                    
                    //an to diavasan oli to diagrafoume
                    if (g_shm->msgs[mi].remaining_reads==0) {
                        g_shm->msgs[mi].used=0; 
                    }
                }

                sender =g_shm->msgs[mi].sender; 
                dialog_id =g_shm->msgs[mi].dialog_id; 
                strncpy(local_payload,g_shm->msgs[mi].payload,MAX_PAYLOAD); 
                local_payload[MAX_PAYLOAD -1] ='\0'; 
                sem_post(&g_shm->mutex);

                /*χρησιμοποιώ mutex στην εκτύπωση για να μην μπλεχτεί το μήνυμα 
                   με αυτά που πληκτρολογεί ο χρήστης εκείνη την ώρα.*/
                pthread_mutex_lock(&g_print_mutex); 
                printf("\n--- New message (dialog %d) ---\nFrom PID: %d\n%s\n-----------------------------\n",
                       dialog_id, sender, local_payload); 
                pthread_mutex_unlock(&g_print_mutex); 

                local_payload[strcspn(local_payload,"\r\n")] ='\0'; 
                if (strcmp(local_payload,"TERMINATE")==0) { 

                    /*βγαίνω από τον διάλογο και ρίχνω το flag (g_running)
                    για να σταματήσει και το main thread.*/
                    sem_wait(&g_shm->mutex); 
                    
                    int my_dialog_id=find_active_dialog_id_for_pid_locked(g_shm,g_pid);
                    if (my_dialog_id!=-1) {
                         leave_dialog_locked(g_shm,my_dialog_id,g_pid);
                    }
                    
                    cleanup_dead_participants_locked(g_shm); 
                    sem_post(&g_shm->mutex); 

                    maybe_unlink_shm_if_no_active(g_shm); 

                    g_running = 0; 
                    break; 
                }
            }
        } 

        else if (g_running) {
            /*δεν έχω μηνύματα, οπότε "κοιμάμαι" εδώ για να μην καίω τσάμπα CPU (no busy waiting).
            *θα με ξυπνήσει ο sender με broadcast μόλις στείλει κάτι.
            */
            pthread_mutex_lock(&g_shm->cv_mutex);
            
            /*περιμένω για σήμα*/
            pthread_cond_wait(&g_shm->cv_newmsg,&g_shm->cv_mutex); 
            
            pthread_mutex_unlock(&g_shm->cv_mutex); 
        }
    }

    return NULL;
}