#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "shared.h"
#include "dialog.h"
#include "message.h"
#include "receiver.h"

shm_region_t *g_shm= NULL;
pid_t g_pid= 0;
volatile int g_running= 1;//εβαλα volatile γιατι αποτρέπει το caching.Δοκίμασα με σκέτο int και δεν δούλεψε (το loop δεν σταματούσε).
pthread_mutex_t g_print_mutex= PTHREAD_MUTEX_INITIALIZER;

static void notify_receivers_after_send(void) {//eνημερώνει τους δέκτες μετά την αποστολή μηνύματος,DEN ΠΡΕΠΕΙ ΝΑ ΚΑΛΕΙΤΑΙ ΜΕΣΑ ΣΕ SHM->MUTEX
    pthread_mutex_lock(&g_shm->cv_mutex);
    pthread_cond_broadcast(&g_shm->cv_newmsg);
    pthread_mutex_unlock(&g_shm->cv_mutex);
}

int main(void) {
    if (setup_shm_region(&g_shm)!= 0){//ρύθμιση της περιοχής shared memory
        fprintf(stderr,"Failed to setup SHM\n");
        return 1;
    }

    g_pid =getpid();//λήψη του PID της τρέχουσας διεργασίας

    pthread_t recv_thread;//*δημιουργία του νήματος δέκτη */
    if (pthread_create(&recv_thread,NULL,receiver_thread_fn,NULL) != 0) {
        perror("pthread_create");
        teardown_shm_region(g_shm);//*καταστροφή της περιοχής shared memory*/
        return 1;
    }

    pthread_mutex_lock(&g_print_mutex);
    printf("Multi-threaded chat process PID %d\n",g_pid);
    printf("Commands:\n  create\n  join <id>\n  leave <id>\n  send <dialog_id> <text>\n  list\n  exit\n");
    pthread_mutex_unlock(&g_print_mutex);

    char line[1024];
    //περιμένει εντολές από τον χρήστη
    while (g_running){
        pthread_mutex_lock(&g_print_mutex);
        printf("> ");
        fflush(stdout);
        pthread_mutex_unlock(&g_print_mutex);

        //περιμένει εδώ μέχρι να πατηθεί Enter
        if (!fgets(line,sizeof(line),stdin))
            break;
            
        char *nl=strchr(line,'\n');//βρίσκει το newline
        if (nl) *nl ='\0';//αντικαθιστά το newline με null terminator

        if (strncmp(line,"create",6)==0) {
            sem_wait(&g_shm->mutex);
            int id=create_dialog_and_join_locked(g_shm,g_pid);
            sem_post(&g_shm->mutex);

            pthread_mutex_lock(&g_print_mutex);
            if (id==-1) {
                printf("Failed to create dialog\n");
            } else{
                printf("Created dialog %d\n",id);
            }
            pthread_mutex_unlock(&g_print_mutex);

            notify_receivers_after_send();//ειδοποίηση
        }
        else if (strncmp(line,"join ",5)==0) {
            int id=atoi(line +5);//pernw to id pou thelei na kanei join
            sem_wait(&g_shm->mutex);
            int r=join_dialog_locked(g_shm,id,g_pid);
            sem_post(&g_shm->mutex);

            pthread_mutex_lock(&g_print_mutex);
            if (r == 0) printf("Joined %d\n",id);
            else printf("Join failed\n");
            pthread_mutex_unlock(&g_print_mutex);
            //ειδοποιούμε ότι υπάρχει νέο μήνυμα
            notify_receivers_after_send();
        }
        else if (strncmp(line,"leave ",6)==0) {
            int id=atoi(line +6);//pernw to id pou thelei na kanei leave
            sem_wait(&g_shm->mutex);
            int r=leave_dialog_locked(g_shm,id,g_pid);
            sem_post(&g_shm->mutex);

            pthread_mutex_lock(&g_print_mutex);
            if (r==0) {
                printf("Left %d\n",id);
            } else {
                printf("Leave failed\n");
            }
            pthread_mutex_unlock(&g_print_mutex);

            notify_receivers_after_send();
        }
        else if (strncmp(line,"send ",5)==0) {
            char *p =line+5;
            while (*p==' ') 
            { 
                p++; 
            }

            char *sp=strchr(p,' ');
            if (!sp){
                pthread_mutex_lock(&g_print_mutex);
                printf("Usage: send <dialog_id> <text>\n");
                pthread_mutex_unlock(&g_print_mutex);
                continue;
            }
            //auto to kanw etsi wste na kanw extract to id kai to text , diladi an kanw send 1 hello tote to id einai 1 kai to text einai hello
            //ousiastika antikathisto to prwto keno me \0 wste na exw to id ws mia xarakthristikh akolouthia kai meta to keimeno
            *sp ='\0';
            int id =atoi(p);
            char *text =sp + 1;

            /*call locked variant to modify SHM safely*/
            sem_wait(&g_shm->mutex);
            cleanup_dead_participants_locked(g_shm);
            int r =send_message_locked(g_shm,id,g_pid,text);
            sem_post(&g_shm->mutex);

            pthread_mutex_lock(&g_print_mutex);
            if (r==0) {
                printf("Message sent\n");
            } else {
                printf("Send failed\n");
            }
            pthread_mutex_unlock(&g_print_mutex);

            /*notify receivers after we released shm->mutex*/
            notify_receivers_after_send();

            if (strcmp(text,"TERMINATE")==0) {
                /*leave dialog (if still present)*/
                sem_wait(&g_shm->mutex);
                int didx =find_dialog_index_locked(g_shm,id);
                if (didx!=-1)
                    leave_dialog_locked(g_shm,g_shm->dialogs[didx].id,g_pid);
                sem_post(&g_shm->mutex);

                /*attempt unlink if nobody remains*/
                maybe_unlink_shm_if_no_active(g_shm);

                /*set running=0 and wake all waiting receivers*/
                g_running=0;
                notify_receivers_after_send();
                break;
            }
        }
        else if (strcmp(line,"list")==0) {
            sem_wait(&g_shm->mutex);
            cleanup_dead_participants_locked(g_shm);
            list_state_locked(g_shm);
            sem_post(&g_shm->mutex);
        }
        else if (strcmp(line,"exit")==0) { 
            // Βρίσκουμε τον διάλογο που συμμετέχουμε (αν υπάρχει) και φεύγουμε.
            sem_wait(&g_shm->mutex); 
                    
            int my_dialog_id =find_active_dialog_id_for_pid_locked(g_shm,g_pid);
            if (my_dialog_id!=-1) {
                    leave_dialog_locked(g_shm,my_dialog_id,g_pid);
            }

            sem_post(&g_shm->mutex); 
            //Καθαρίζουμε τη μνήμη αν είμαστε οι τελευταίοι
            maybe_unlink_shm_if_no_active(g_shm); 

            g_running=0; 
            notify_receivers_after_send(); 
            break; 
            }
        else {
            pthread_mutex_lock(&g_print_mutex);
            printf("Unknown\n");
            pthread_mutex_unlock(&g_print_mutex);
        }
    }

    //Περιμένουμε το Receiver Thread να κλείσει ομαλά
    pthread_join(recv_thread,NULL);
    teardown_shm_region(g_shm);

    pthread_mutex_lock(&g_print_mutex);
    printf("Exiting %d\n",g_pid);
    pthread_mutex_unlock(&g_print_mutex);

    return 0;
}
