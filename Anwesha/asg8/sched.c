#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <math.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>

struct message {
    long msg_type; // Message type (must be > 0)
    int index; // Process index
};

int main(int argc, char *argv[]){
    int msgid_rq, msgid_smmu, k;
    if(argc != 4){
        printf("Usage: %s <msgid_rq> <msgid_smmu> <num_processes>\n", argv[0]);
        exit(1);
    }

    msgid_rq = atoi(argv[1]);
    msgid_smmu = atoi(argv[2]);
    k = atoi(argv[3]);


    key_t key_master_sched = 8901;
    int semid_master_sched = semget(key_master_sched, 1, 0666|IPC_CREAT);
    struct sembuf sb;

    struct message msg_rq, msg_smmu;

    int key_sched_process = 3462;
    int semid_sched_process = semget(key_sched_process, k, 0666|IPC_CREAT);

    int count = k;

    printf("Scheduler started\n");
    fflush(stdout);
    
    while(count!=0){
        memset(&msg_rq, 0, sizeof(struct message));
        int err = msgrcv(msgid_rq, &msg_rq, sizeof(struct message), 0, 0);
        if (err == -1){
            perror("msgrcv");
            exit(1);
        }
        printf("Scheduler received message from: %d\n", msg_rq.index);
        fflush(stdout);

        //signal the semaphore of the process
        sb.sem_num = msg_rq.index;
        sb.sem_op = 1;
        sb.sem_flg = 0;
        semop(semid_sched_process, &sb, 1);

        msgrcv(msgid_smmu, &msg_smmu, sizeof(struct message), 0, 0);
        printf("Scheduler received message from SMMU\n");
        fflush(stdout);

        //check the message type and the process index
        if(msg_smmu.msg_type == 1){
            msg_rq.msg_type = 1;
            msg_rq.index = msg_smmu.index;
            msgsnd(msgid_rq, &msg_rq, sizeof(struct message) - sizeof(long), 0);
            printf("Process %d readded to queue\n", msg_smmu.index);
            fflush(stdout);
        }else if(msg_smmu.msg_type == 2){
            count--;
            printf("Process %d finished\n", msg_smmu.index);
            fflush(stdout);
        }
    }

    printf("Scheduler finished\n");
    fflush(stdout);

    // Signal Master to continue
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(semid_master_sched, &sb, 1);
}