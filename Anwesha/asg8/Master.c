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
#include <time.h>

#define LOW_PROB 1
#define BIG_NUM 1000

typedef struct {
    int frame; // Frame number for the page
    int valid; // 0 for invalid, 1 for valid
    int time; // Time of last access
} pt_entry;

struct message {
    long msg_type; // Message type (must be > 0)
    int index; // Process index
};

int main(int argc, char * argv[]){
    srand(time(0));

    int k, m, f;

    printf("Enter the number of processes: ");
    scanf("%d", &k);
    printf("Enter the number of pages per process: ");
    scanf("%d", &m);
    printf("Enter the number of frames: ");
    scanf("%d", &f);

    //create shared memory to store the number of pages for each process
    key_t key_vm = 1234;
    int shmid_vm = shmget(key_vm, k * sizeof(int), 0777|IPC_CREAT);
    if (shmid_vm == -1){
        perror("shmget");
        exit(1);
    }

    int * vm = (int *)shmat(shmid_vm, NULL, 0);
    if (vm == (void *)-1){
        perror("shmat");
        exit(1);
    }
    memset(vm, 0, k * sizeof(int));
    // int pm[k];
    int sum = 0;    //this is sum of page

    //select a random number of pages for each process
    for(int i = 0; i < k; i++){
        vm[i] = (rand() % m) + 1;
        sum = sum + vm[i];
    }

    //print the results
    for(int i = 0; i < k; i++){
        printf("Process %d: Virtual Memory Space = %d\n", i, vm[i]);
        fflush(stdout);
    }

    int maxlen = 10*m; //max length of reference string

    //create a reference string for each process
    int rstr[k][maxlen];

    for(int i = 0; i < k; i++){
        int len = (rand() % (10*vm[i] - 2*vm[i] + 1) + 2*vm[i]); //select a random number between 2*vm[i] and 10*vm[i], including both
        printf("len of process %d: %d\n", i, len);
        fflush(stdout);
        for(int j = 0; j < maxlen; j++){
            if(j < len){
                //select a random page number between 0 and vm[i] - 1
                rstr[i][j] = rand() % vm[i];
            }else{
                rstr[i][j] = -1; //illegal as the reference string is shorter
            }                
        }
    }

    for(int i = 0; i < k; i++){
        for(int j = 0; j < maxlen; j++){
            if(rstr[i][j] == -1){
                break;
            }else{
                if(rand() % 100 < LOW_PROB){
                    printf("Process %d: Page %d is invalid/illegal\n", i, j);
                    fflush(stdout);
                    //choose some random value > m
                    if(rand() % 2 == 0){
                        rstr[i][j] = vm[i] + rand() % vm[i] ;
                    }else{
                        //a random value > m and < 2m
                        rstr[i][j] = m + rand() % m;
                    }

                }   
            }
        }
    }

    //print the reference strings
    for(int i = 0; i < k; i++){
        printf("Reference String for Process %d: ", i);
        fflush(stdout);
        int j = 0;
        while(rstr[i][j] != -1){
            printf("%d ", rstr[i][j]);
            j++;
        }
        printf("\n");
        fflush(stdout);
    }

    //wait for scheduler to signal it
    int key_sched_process = 3462;
    int semid_sched_process = semget(key_sched_process, k, 0666|IPC_CREAT);
    //Initializing the semaphores to 0
    for(int i = 0; i < k; i++){
        semctl(semid_sched_process, i, SETVAL, 0);
    }


    //create shared memory for page tables
    key_t key_pt = 2389;        //ftok("process.c", 65);
    //First delete the shared memory if it already exists
    int shmid_pt = shmget(key_pt, sum * sizeof(pt_entry), 0777|IPC_CREAT);
    shmctl(shmid_pt, IPC_RMID, NULL);
    shmid_pt = shmget(key_pt, sum * sizeof(pt_entry), 0777|IPC_CREAT);
    if (shmid_pt == -1){
        perror("shmget");
        exit(1);
    }

    pt_entry *pt = (pt_entry *)shmat(shmid_pt, NULL, 0);

    for(int i = 0; i < sum; i++){
        pt[i].frame = -1;
        pt[i].valid = 0;
        pt[i].time = BIG_NUM;
    }

    //create shared memory for free frame list
    int key_ff = 3456;        //ftok("process.c", 66);
    int shmid_ff = shmget(key_ff, f * sizeof(int), 0777|IPC_CREAT);

    int *freeframe_list = (int *)shmat(shmid_ff, NULL, 0);

    for(int i = 0; i < f; i++){
        freeframe_list[i] = 1; //1 indicates free frame, 0 indicates occupied frame
    }


    //creade a ready queue to be used by the scheduler using message queue
    key_t key_rq = 4567;        //ftok("process.c", 67);
    int msgid_rq = msgget(key_rq, 0777|IPC_CREAT);
    //Cleaning the message queue
    struct message msg_rq;
    while(msgrcv(msgid_rq, &msg_rq, sizeof(struct message) - sizeof(long), 0, IPC_NOWAIT) != -1);
    printf("msgid_rq: %d\n", msgid_rq);


    //create a message queue for communication between scheduler and mmu
    key_t key_smmu = 6789;        //ftok("mmu.c", 65);
    int msgid_smmu = msgget(key_smmu, 0777|IPC_CREAT);
    while (msgrcv(msgid_smmu, &msg_rq, sizeof(struct message) - sizeof(long), 0, IPC_NOWAIT) != -1);
    printf("msgid_smmu: %d\n", msgid_smmu);


    //create a message queue for communication between process and mmu
    key_t key_pmmu = 7890;        //ftok("mmu.c", 66);
    int msgid_pmmu = msgget(key_pmmu, 0777|IPC_CREAT);
    while (msgrcv(msgid_pmmu, &msg_rq, sizeof(struct message) - sizeof(long), 0, IPC_NOWAIT) != -1);
    printf("msgid_pmmu: %d\n", msgid_pmmu);
    fflush(stdout);

    //create semaphore for master to wait for scheduler to notify that all processes have completed
    key_t key_master_sched = 8901;
    int semid_master_sched = semget(key_master_sched, 1, 0666|IPC_CREAT);
    semctl(semid_master_sched, 0, SETVAL, 0); //initialize semaphore value to 0


    //create scheduler process, and pass msgid_rq, msgid_smmu as arguments
    int pid_s = fork();
    if(pid_s == 0){
        char* str1 = (char *)malloc(30);
        char* str2 = (char *)malloc(30);
        char* str3 = (char *)malloc(30);
        sprintf(str1, "%d", msgid_rq);
        sprintf(str2, "%d", msgid_smmu);
        sprintf(str3, "%d", k);
        char* args[] = {"./sched", str1, str2, str3, NULL};
        execvp(args[0], args);
    }

    //mmu process
    int pid_m = fork();
    if(pid_m == 0){
        char* str1 = (char *)malloc(30);
        char* str2 = (char *)malloc(30);
        char* str3 = (char *)malloc(30);
        char* str4 = (char *)malloc(30);
        char* str5 = (char *)malloc(30);
        char* str6 = (char *)malloc(30);
        char* str7 = (char *)malloc(30);

        sprintf(str1, "%d", msgid_smmu);
        sprintf(str2, "%d", msgid_pmmu);
        sprintf(str3, "%d", shmid_pt);
        sprintf(str4, "%d", shmid_ff);
        sprintf(str5, "%d", shmid_vm);
        sprintf(str6, "%d", k);
        sprintf(str7, "%d", f);

        char* args[] = {"xterm", "-hold", "-e", "./mmu", str1, str2, str3, str4, str5, str6, str7, NULL};
        //f is to have length of shmid_ff, k is vm len, sum of vm is length of pt
        execvp(args[0], args);
    }
    // printf("mmu created\n");
    // fflush(stdout);


    //create k processes at a time interval of 250ms, and pass msgid_rq, msgid_pmmu, rstr[i] as arguments
    for(int i = 0; i < k; i++){
        int pid = fork();
        if(pid == 0){ 
            char* str1 = (char *)malloc(30);
            char* str2 = (char *)malloc(30);
            char* str3 = (char *)malloc(1000);
            char* str4 = (char *)malloc(30);
            char* str5 = (char *)malloc(30);

            sprintf(str1, "%d", msgid_rq);
            sprintf(str2, "%d", msgid_pmmu);
            //Creating a string of reference string, delimited by '.'
            for(int j = 0; j < maxlen; j++){
                char temp[10];
                sprintf(temp, "%d", rstr[i][j]);
                strcat(str3, temp);
                if(j != maxlen - 1){
                    strcat(str3, ".");
                }
            }

            sprintf(str4, "%d", i);
            sprintf(str5, "%d", k);

            char* args[] = {"./process", str1, str2, str3, str4, str5, NULL};
            execvp(args[0], args);
        }
        usleep(250000);
    }

    printf("master all done\n");
    fflush(stdout);

    //wait until scheduler notifies that all processes have completed
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;

    semop(semid_master_sched, &sb, 1); //wait for scheduler to notify that all processes have completed
    printf("master received signal from scheduler\n");
    fflush(stdout);


    //terminate scheduler and memory manager
    kill(pid_s, SIGTERM);
    kill(pid_m, SIGTERM);

    //detach shared memory
    shmdt(vm);
    shmdt(pt);
    shmdt(freeframe_list);


    //destroy shared memory
    shmctl(shmid_vm, IPC_RMID, NULL);
    shmctl(shmid_pt, IPC_RMID, NULL);
    shmctl(shmid_ff, IPC_RMID, NULL);

    //destroy message queues
    msgctl(msgid_rq, IPC_RMID, NULL);
    msgctl(msgid_smmu, IPC_RMID, NULL);
    msgctl(msgid_pmmu, IPC_RMID, NULL);

    //destroy semaphore
    semctl(semid_master_sched, 0, IPC_RMID);

    return 0;

}