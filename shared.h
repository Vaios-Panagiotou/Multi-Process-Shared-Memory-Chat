#ifndef SHARED_H
#define SHARED_H


#include <semaphore.h>
#include <sys/types.h>
#include <pthread.h>


#define SHM_NAME "/chat_shm_21133"
#define MAX_DIALOGS 16//evala 16 giati theorisa oti einai uperarketo gia na kaneis megala test
#define MAX_PARTICIPANTS 64//evala 64 ws ena anw orio se periptwsh pou theleis na kaneis stress test na min kanei overflow to systima
#define MAX_MSGS 512//ousiastika einai 512/16 =32 mhnymata ana dialogo pou mporoun na apothikeutoun poy to theoro arketo gia test
#define MAX_PAYLOAD 256//theorisa oti ta munimata 8a einai mikra tou tipo , geia , ti kaneis , kai etsi tha prepei na einai arketa


typedef struct {//*domi dialogou*/
int id;//*dialog id*/
int active; /* 0/1 */
int participant_count;//*arithmos symmetexontwn*/
pid_t participants[MAX_PARTICIPANTS];//*pinakas me ta pids twn symmetexontwn*/
} dialog_t;


typedef struct {//*domi minimatos*/
int used; /* 0/1 */
int msg_id;//**id minimatos*/
int dialog_id;//*id dialogou sto opoio anhkei to minimma*/
pid_t sender;//*pid apostolea*/
unsigned int remaining_reads;//*arithmos symmetexontwn pou den exoun diavasei to minimma akoma,evala unsigned int giati den mporoun na einai arnhtikos o arithmos,pio polu gia sigouria dikia moy , theoritika den 8a ginei pote arnitiko*/
char payload[MAX_PAYLOAD];//*periexomeno minimatos*/
} message_t;


typedef struct {
int initialized;//1 an exei ftiaxtei to shm 0 an den exei
sem_t mutex; /*process-shared semaphore*/
pthread_mutex_t cv_mutex;//mutex για condition variable
pthread_cond_t  cv_newmsg;//condition variable για "νέο μήνυμα"
int next_dialog_id;//*επόμενο διαθέσιμο dialog ID*/
int next_msg_id;//*επόμενο διαθέσιμο message ID*/
dialog_t dialogs[MAX_DIALOGS];//*pinakas dialogwn*/
message_t msgs[MAX_MSGS];//*pinakas minimatwn*/
} shm_region_t;





int setup_shm_region(shm_region_t **out_shm);
void teardown_shm_region(shm_region_t *shm);
int find_dialog_index_locked(shm_region_t *shm,int dialog_id);
int create_dialog_and_join_locked(shm_region_t *shm,pid_t pid);
int join_dialog_locked(shm_region_t *shm,int dialog_id,pid_t pid);
int leave_dialog_locked(shm_region_t *shm,int dialog_id,pid_t pid);
int send_message_locked(shm_region_t *shm,int dialog_id,pid_t sender,const char *payload);
int collect_messages_for_pid(shm_region_t *shm,pid_t pid,int *out_idxs,int max_out);
int list_state_locked(shm_region_t *shm);
void maybe_unlink_shm_if_no_active(shm_region_t *shm);
void cleanup_dead_participants_locked(shm_region_t *shm);
int is_pid_in_any_dialog_locked(shm_region_t *shm, pid_t pid);
int find_active_dialog_id_for_pid_locked(shm_region_t *shm, pid_t pid);
#endif 