#include "shared.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>


int find_dialog_index_locked(shm_region_t *shm,int dialog_id) {
    for(int i=0;i< MAX_DIALOGS;++i)
        if(shm->dialogs[i].active &&shm->dialogs[i].id== dialog_id)//ean o dialog einai active kai to id tou einai iso me to dialog_id
            return i;
    return -1;
}

int is_pid_in_any_dialog_locked(shm_region_t *shm,pid_t pid) {//ψαχνει αν το pid ειναι σε καποιον διαλογο και επιστρεφει 1 αν το βρει και 0 αν δεν το βρει
    for (int i= 0;i < MAX_DIALOGS;++i) {
        if (!shm->dialogs[i].active) 
        {
            continue;
        }
        dialog_t *d =&shm->dialogs[i];
        for (int j= 0;j< d->participant_count;++j) {
            if (d->participants[j]== pid) {
                return 1;//brthike
            }
        }
    }
    return 0;//den brethike
}


int find_active_dialog_id_for_pid_locked(shm_region_t *shm,pid_t pid) {//βρίσκει τον ενα διάλογο στον οποίο συμμετέχει ένα pid και επιστρέφει το dialog ID,ή -1 αν δεν είναι σε κανέναν.
    for (int i= 0;i < MAX_DIALOGS;++i) {
        if (!shm->dialogs[i].active) 
        {
            continue;
        }
        dialog_t *d=&shm->dialogs[i];
        for (int j= 0;j< d->participant_count;++j) {
            if (d->participants[j] ==pid) {
                return d->id; //βρέθηκε
            }
        }
    }
    return -1; //δεν είναι πουθενά
}
/*συναρτησεις διαλογων*/
int create_dialog_and_join_locked(shm_region_t *shm,pid_t pid) {//δημιουργεί έναν νέο διάλογο και προσθέτει το pid ως συμμετέχοντα

    if (is_pid_in_any_dialog_locked(shm,pid)) {//έλεγχος αν το pid είναι ήδη σε κάποιον διάλογο
        return -1; //το pid είναι ήδη σε διάλογο
    }

    int idx=-1;
    for (int i= 0;i <MAX_DIALOGS;++i){
        if (!shm->dialogs[i].active)//βρίσκει μια ελεύθερη θέση για νέο διάλογο
        {
            idx=i;//ορισμός της θέσης
            break; 
        }
    }
    if (idx==-1){
        return -1; //Δεν υπάρχει διαθέσιμος χώρος για νέο διάλογο
    }

    int id= shm->next_dialog_id++; //αύξηση του επόμενου διαθέσιμου dialog ID

    dialog_t *d= &shm->dialogs[idx]; 
    d->id= id; 
    d->active= 1; 
    d->participant_count= 0; 
    d->participants[d->participant_count++]= pid; 

    return id; 
}

int join_dialog_locked(shm_region_t *shm,int dialog_id,pid_t pid) {

    if (is_pid_in_any_dialog_locked(shm, pid)) {
        return -1; //Το pid είναι ήδη σε διάλογο
    }

    int idx =find_dialog_index_locked(shm,dialog_id); //βρίσκει τον διάλογο με το συγκεκριμένο dialog_id
    if (idx==-1){ 
        return -1; //Δεν βρέθηκε ο διάλογος
    }
    dialog_t *d=&shm->dialogs[idx];

    if (d->participant_count>=MAX_PARTICIPANTS)//έλεγχος αν ο διάλογος είναι γεμάτος
        return -1; //Ο διάλογος είναι γεμάτος


    d->participants[d->participant_count++]= pid;
    return 0;
}
int leave_dialog_locked(shm_region_t *shm,int dialog_id,pid_t pid) {
    int idx=find_dialog_index_locked(shm,dialog_id);//βρίσκει τον διάλογο με το συγκεκριμένο dialog_id
    if (idx==-1){//Δεν βρέθηκε ο διάλογος
     return -1;
    }

    dialog_t *d =&shm->dialogs[idx];//παίρνει τον διάλογο

    int found= -1;
    for (int i=0;i< d->participant_count;++i) {
        if (d->participants[i]==pid) {
            found =i;
            break;
        }
    }

    if (found==-1){
        return -1;
    }
    /*γυρναω τα υπόλοιπα στοιχεία αριστερά*/
    for (int i =found;i< d->participant_count- 1;++i)//μετακινει τα υπολοιπα στοιχεια αριστερα
        d->participants[i]= d->participants[i+1];

    d->participant_count--;//μειωνει τον αριθμο των συμμετεχοντων κατα 1 γιατι βγαζει τον εαυτο του

    if (d->participant_count== 0)//αν δεν υπαρχει αλλος συμμετεχων κανει τον διαλογο ανενεργο
        d->active = 0;

    return 0;
}

int send_message_locked(shm_region_t *shm,int dialog_id,pid_t sender,const char *payload) {//*στελνει μηνυμα σε συγκεκριμενο διαλογο με συγκεκριμενο αποστολεα*/
    int didx = find_dialog_index_locked(shm,dialog_id);//βρίσκει τον διάλογο με το συγκεκριμένο dialog_id
    if (didx ==-1){//Δεν βρέθηκε ο διάλογος
        return -1;
    }

    dialog_t *d =&shm->dialogs[didx];//παίρνει τον διάλογο



    int is_member =0;
    for (int i= 0; i< d->participant_count;++i) {
        if (d->participants[i]== sender) {
            is_member= 1;
            break;
        }
    }

    if (!is_member) {
        return -1; //δεν είσαι μέλος,απαγορεύεται να στείλεις
    }
    
    /*vres message slot*/
    int midx= -1;
    for (int i= 0;i< MAX_MSGS;++i)
        if (!shm->msgs[i].used) 
        { 
            midx=i; 
            break; 
        }

    if (midx==-1) return -1;

    message_t *m =&shm->msgs[midx];//παίρνει το μήνυμα
    m->used=1;
    m->msg_id=shm->next_msg_id++;
    m->dialog_id=dialog_id;
    m->sender=sender;
    
    int readers_count =0;
    
    //lopparw mexri to participant_count
    for (int i =0; i <d->participant_count;++i) {
        //an to pid den einai o apostoleas
        if (d->participants[i]!=sender) {
            readers_count++;
        }
    }

    m->remaining_reads=readers_count;
    strncpy(m->payload,payload,MAX_PAYLOAD - 1);//αντιγραφή του περιεχομένου στο payload
    m->payload[MAX_PAYLOAD - 1]='\0';//διασφαλίζει ότι το payload είναι null-terminated


    return 0;
}

int collect_messages_for_pid(shm_region_t *shm,pid_t pid,int *out_idxs,int max_out) {//*wrapper για τη συλλογή μηνυμάτων για ένα συγκεκριμένο PID.*/
    int cnt = 0;//μετρητής για τα μηνύματα που έχουν συλλεχθεί

    sem_wait(&shm->mutex);//κλειδώνει την πρόσβαση στη μοιραζόμενη μνήμη

    /*βρες τον ενα διάλογο (αν υπάρχει) στον οποίο είναι το pid.*/
    int my_dialog_id =find_active_dialog_id_for_pid_locked(shm,pid);

    if (my_dialog_id==-1){
        sem_post(&shm->mutex);
        return 0; //δεν είναι σε κανέναν διάλογο,άρα 0 μηνύματα.
    }

    /*σάρωσε τα μηνύματα.*/
    for (int i = 0;i < MAX_MSGS; ++i) { 
        if (!shm->msgs[i].used) continue; 
        message_t *m =&shm->msgs[i]; 

        if (m->dialog_id== my_dialog_id&& m->sender!= pid) {
            
            
            if (m->remaining_reads== 0) 
                continue; 

            if (cnt < max_out) 
                out_idxs[cnt]=i; 

            cnt++; 
        }
    }

    sem_post(&shm->mutex); 

    return (cnt <max_out) ? cnt : max_out; 
}



////SUNEXEIA EDW AURIO 


/*
 * την καλω ΜΟΝΟ αν έχουμε κλειδώσει ήδη (sem_wait), αλλιώς 
 * μπορεί να τυπώσουμε δεδομένα που αλλάζουν εκείνη τη στιγμή.
 */
int list_state_locked(shm_region_t *shm) {
    printf("\n== Dialogs ==\n");
    //lopparo τον στατικό πίνακα διαλόγων
    for (int i = 0; i < MAX_DIALOGS; ++i)
        if (shm->dialogs[i].active) {//Αγνοω τα κενά slots
            printf("Dialog id=%d participants=%d [",
                shm->dialogs[i].id,
                shm->dialogs[i].participant_count);
            /*εμφάνιση των PIDs για να βλέπω ποιοι είναι μέσα*/
            for (int j = 0; j < shm->dialogs[i].participant_count;++j) {
                printf("%d",shm->dialogs[i].participants[j]);
                if (j + 1 <shm->dialogs[i].participant_count)
                    printf(", ");
            }

            printf("]\n");
        }

    printf("== Messages ==\n");
    //σαρώνω τα μηνύματα για να δω τι έχειξεμείνει στη μνήμη
    for (int i= 0; i<MAX_MSGS;++i)
        if (shm->msgs[i].used) {
            /*το πιο ημαντικό που εχω γραψει εδω είναι το 'remaining_reads'.
             *με αυτό ελέγχω αν ο μετρητής μειώνεται σωστά.
             *επίσης κόβω το payload στους 20 χαρακτήρες για να μην γεμίζει η οθόνη.
             */
            printf("msg_id=%d dialog=%d sender=%d remaining=%u payload=\"%.20s%s\"\n",
                shm->msgs[i].msg_id,
                shm->msgs[i].dialog_id,
                shm->msgs[i].sender,
                shm->msgs[i].remaining_reads,
                shm->msgs[i].payload,
                (strlen(shm->msgs[i].payload) > 20 ? "..." : ""));
        }

    return 0;
}

/* 
     * σαρώνω τους διαλόγους για να βρω PIDs που κράσαραν(π.χ.με kill -9)
     * και δεν πρόλαβαν να κάνουν leave,ώστε να μην κολλάνε τα μηνύματα.
     */
void cleanup_dead_participants_locked(shm_region_t *shm) {
    for (int i= 0; i< MAX_DIALOGS;++i) {
        dialog_t *d =&shm->dialogs[i];
        if (!d->active) continue;

        int write_idx= 0;//δείκτης για να ξαναγράψουμε τη λίστα χωρίς τα κενά (compaction)

        for (int j= 0;j < d->participant_count;++j) {
            pid_t p=d->participants[j];

            if (p== 0) continue;
            /*
            * στέλνω σήμα 0.δεν κάνει τίποτα στη διεργασία,αλλά
            * το λειτουργικό μου επιστρέφει λάθος (ESRCH) αν η διεργασία δεν υπάρχει πια.
            */
            if (kill(p, 0) == -1 && errno == ESRCH)
                continue;   /*dead PID → skip*/

            /*αν ζει,την κρατάμε και τη μετακινούμε στη σωστή θέση*/
            if (write_idx!= j)
                d->participants[write_idx]= d->participants[j];

            write_idx++;
        }

        d->participant_count= write_idx;//ενημερώνουμε το νέο πλήθος

        /*αν έφυγαν ή πέθανανόλοι,ο διάλογος κλείνει αυτόματα*/
        if (d->participant_count== 0)
            d->active= 0;
    }
}

/* ----------------- SHM UNLINK LOGIC ------------------- */

void maybe_unlink_shm_if_no_active(shm_region_t *shm) {
    int any= 0;

    sem_wait(&shm->mutex);//κλειδώνω για να ελέγξω την κατάσταση με ασφάλεια

    /*πρώτα καθαρίζω τυχόν νεκρές διεργασίες.
    *αν ο τελευταίος χρήστης κράσαρε,
    *πρέπει να το μάθω τώρα για να σβήσω τη μνήμη.
    */

    cleanup_dead_participants_locked(shm);

    for (int i= 0;i < MAX_DIALOGS;++i)
        if (shm->dialogs[i].active) {
            any= 1;//υπάρχει ακόμα κόσμος, δεν σβήνουμε τίποτα
            break;
        }

    sem_post(&shm->mutex);
    /*
    *αν δεν έμεινε κανένας ενεργός διάλογος, διαγράφω
    *το αρχείο από το /dev/shm για να μην μείνουν σκουπίδια στο σύστημα.
    */
    if (!any)
        shm_unlink(SHM_NAME);
}

/* -----------------SHM SETUp------------------- */

int setup_shm_region(shm_region_t **out) {
    /*δημιουργία ή άνοιγμα του αντικειμένου κοινόχρηστης μνήμης.
     *εβαλα O_CREAT γιατι αν δεν υπάρχει shm,φτιάξτο.
     */
    int fd = shm_open(SHM_NAME,O_RDWR | O_CREAT,0666);
    if (fd < 0) return -1;

    /*καθορισμός μεγέθους.
     αν μόλις φτιάχτηκε, έχει μέγεθος 0, οπότε πρέπει να το μεγαλώσουμε
     για να χωρέσει το struct μας.
     */
    if (ftruncate(fd,sizeof(shm_region_t)) != 0) {
        close(fd);
        return -1;
    }

    /*
     με το MAP_SHARED οι αλλαγές που κάνω εγώ,
     θα φαίνονται αυτόματα και στις άλλες διεργασίες.
     */
    shm_region_t *shm=mmap(NULL,sizeof(shm_region_t),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,fd,0);
    close(fd);/*ο file descriptor δεν χρειάζεται πια μετά το mmap, τον κλείνουμε*/

    if (shm==MAP_FAILED)
        return -1;
    /*
        *εδώ λύνω το Race Condition της αρχικοποίησης.
        *μόνο η ΠΡΩΤΗ διεργασία που θα τρέξει θα δει initialized != 1.
        *αυτή θα αναλάβει να στήσει τα semaphores/mutexes.
        *οι επόμενες θα το βρουν έτοιμο και θα προσπεράσουν αυτό το block.
        */
    if (shm->initialized != 1) {
        memset(shm,0,sizeof(shm_region_t));

        /*το '1' στη μέση σημαίνει process shared).
         *απαραίτητο για να δουλεύει ο σηματοφορέας ανάμεσα σε διεργασίες.*/
        if (sem_init(&shm->mutex,1,1) != 0) {
            munmap(shm,sizeof(shm_region_t));
            return -1;
        }

        /*
         εδώ ρυθμίζω ρητά ως PTHREAD_PROCESS_SHARED για να
         μπορώ να κάνω signal/wait από άλλη διεργασία.
         */
        pthread_mutexattr_t mattr;
        pthread_condattr_t  cattr;

        pthread_mutexattr_init(&mattr);
        pthread_mutexattr_setpshared(&mattr,PTHREAD_PROCESS_SHARED);

        pthread_mutex_init(&shm->cv_mutex,&mattr);

        pthread_condattr_init(&cattr);
        pthread_condattr_setpshared(&cattr,PTHREAD_PROCESS_SHARED);

        pthread_cond_init(&shm->cv_newmsg,&cattr);

        /* ---------------------------------------- */

        shm->next_dialog_id=1;
        shm->next_msg_id=1;
        shm->initialized=1;/*σηκώνω τη σημαία ότι όλα είναι έτοιμα*/
    }

    *out = shm;
    return 0;
}



void teardown_shm_region(shm_region_t *shm) {
    /*απλή αποσύνδεση (unmap) από την τρέχουσα διεργασία.
     *δεν διαγράφει τη μνήμη από το σύστημα (αυτό το κάνει η shm_unlink αλλού).
     */
    if (shm)
        munmap(shm,sizeof(shm_region_t));
}
