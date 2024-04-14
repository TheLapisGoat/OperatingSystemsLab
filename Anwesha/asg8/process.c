#include<stdio.h>
#include<sys/ipc.h>
#include <sys/msg.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>

struct message {
    long msg_type; // Message type (must be > 0)
    int index; // Message content
};

struct msg_pmmu{
    long msg_type;
    int index;
    int page;
};

int main(int argc, char* argv[]) {
    if(argc != 6){
        printf("Usage: %s <msgid_rq> <msgid_pmmu> <rstr> <index> <k>\n", argv[0]);
        exit(1);
    }

    int msgid_rq = atoi(argv[1]);
    int msgid_pmmu = atoi(argv[2]);

    int *rstr = (int*)malloc(500 * sizeof(int));
    //tokenise the reference string
    char *token = strtok(argv[3], ".");
    int i = 0;
    while(token != NULL){
        rstr[i] = atoi(token);
        token = strtok(NULL, ".");
        i++;
    }
    // printf("Reference string: ");
    // for(int j = 0; j < i; j++){
    //     printf("%d ", rstr[j]);
    // }

    int index = atoi(argv[4]);
    int k = atoi(argv[5]);
    struct message msg_rq;

    //add itself to ready queue
    msg_rq.msg_type = 1;
    msg_rq.index = index;

    int err = msgsnd(msgid_rq, &msg_rq, sizeof(struct message) - sizeof(long), 0);
    
    printf("process %d added to queue\n", index);
    fflush(stdout);

    //wait for scheduler to signal it
    int key_sched_process = 3462;
    int semid_sched_process = semget(key_sched_process, k, 0666);
    struct sembuf sb;

    sb.sem_num = index;
    sb.sem_op = -1;
    sb.sem_flg = 0;

    semop(semid_sched_process, &sb, 1); //wait
    printf("process %d signalled by scheduler\n", index);
    fflush(stdout);

    //send refstr to mmu
    int refindex = 0;

    //message sent to mmu has type 1, message received from mmu has type 2
    struct msg_pmmu msg_for_mmu, msg_from_mmu;
    while(rstr[refindex] != -1){
        msg_for_mmu.msg_type = 1;
        msg_for_mmu.index = index;
        msg_for_mmu.page = rstr[refindex];

        msgsnd(msgid_pmmu, &msg_for_mmu, sizeof(struct msg_pmmu) - sizeof(long), 0);

        msgrcv(msgid_pmmu, &msg_from_mmu, sizeof(struct msg_pmmu), 2, 0);

        if(msg_from_mmu.page == -1){
            //page fault
            printf("Page fault\n");
            int temp = rstr[refindex]; //save the page number
            sb.sem_num = index;
            sb.sem_op = -1;
            sb.sem_flg = 0;

            semop(semid_sched_process, &sb, 1); //wait for scheduler
            printf("process %d resignalled by scheduler\n", index);
            fflush(stdout);

            rstr[refindex] = temp; //restore the page number

        }else if(msg_from_mmu.page == -2){
            //invalid reference
            printf("Invalid reference\n");
            break;
        }else{
            refindex++;
        }     
    }

    //send termination signal
    if(msg_from_mmu.page == -2){
        exit(0);
    }else if(rstr[refindex] == -1){
        msg_for_mmu.msg_type = 1;
        msg_for_mmu.index = index;
        msg_for_mmu.page = -9;

        msgsnd(msgid_pmmu, &msg_for_mmu, sizeof(struct msg_pmmu) - sizeof(long), 0);
    }else{
        printf("Error\n");
        exit(1);
    }

}
