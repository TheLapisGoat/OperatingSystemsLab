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
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>

#define BIG_NUM 1000

typedef struct {
    int frame; // Frame number for the page
    int valid; // 0 for invalid, 1 for valid
    int time; // Time of last access
}pt_entry;

struct message {
    long msg_type; // Message type (must be > 0)
    int index; // Message content
};

struct msg_pmmu{
    long msg_type;
    int index;
    int page;
};

int count = 0; //global count of page references

void main(int argc, char* argv[]) {

    if(argc != 8){
        printf("Usage: %s <msgid_smmu> <msgid_pmmu> <shmid_pt> <shmid_ff> <shmid_vm> <k> <f>\n", argv[0]);
        exit(1);
    }

    int msgid_smmu = atoi(argv[1]); //communication between sched and mmu
    int msgid_pmmu = atoi(argv[2]); //communication between proc and mmu
    int shmid_pt = atoi(argv[3]); //page table
    int shmid_ff = atoi(argv[4]); //free frame list
    int shmid_vm = atoi(argv[5]); //pages per process
    int k = atoi(argv[6]); //num of processes
    int f = atoi(argv[7]); //num of frames

    //Print all args in 1 line
    // printf("mmu: %d %d %d %d %d %d %d\n", msgid_smmu, msgid_pmmu, shmid_pt, shmid_ff, shmid_vm, k, f);
    // fflush(stdout);

    int* vm = (int*)shmat(shmid_vm, NULL,0);

    pt_entry* pt = (pt_entry*) shmat(shmid_pt,NULL,0);

    int* freeframe_list = (int*) shmat(shmid_ff,NULL, 0);

    struct msg_pmmu msg_from_proc, msg_for_proc;

    struct message msg_for_sched;

    int c_vm[300]; //cumulative vm[] where ith is sum till i-1
    memset(c_vm, 0, 300*sizeof(int));
    c_vm[0] = 0;
    for (int i = 1; i<k; i++) {
        c_vm[i] = c_vm[i-1] + vm[i-1];
    }

    while(1){
        //messsage sent to process has type 2, message received from process has type 1
        msgrcv(msgid_pmmu, &msg_from_proc, sizeof(struct msg_pmmu), 1, 0);
        //Print all msg details in 1 line
        printf("mmu: %ld %d %d\n", msg_from_proc.msg_type, msg_from_proc.index, msg_from_proc.page);
        fflush(stdout);

        if(msg_from_proc.page==-9){
            //process has terminated
            //update the free frame list
            for(int i = c_vm[msg_from_proc.index]; i < c_vm[msg_for_proc.index] + vm[msg_for_proc.index]; i++){
                if(pt[i].valid==1){
                    pt[i].valid = 0;
                    freeframe_list[pt[i].frame] = 1;
                    pt[i].frame = -1;
                    pt[i].time = BIG_NUM;
                }
            }
            //send type 2 message to scheduler
            msg_for_sched.msg_type = 2;
            msg_for_sched.index = msg_from_proc.index;

            msgsnd(msgid_smmu, &msg_for_sched, sizeof(struct message) - sizeof(long), 0);
            printf("Process %d terminated\n", msg_from_proc.index);
            fflush(stdout);
        }else{
            //check if page is valid
            count++;
            if(msg_from_proc.page < 0 || msg_from_proc.page >= vm[msg_from_proc.index]){
                //invalid page
                printf("mmu: invalid reference: %d, %d, %d\n", msg_from_proc.index, msg_from_proc.page, vm[msg_from_proc.index]);
                fflush(stdout);
                msg_for_proc.msg_type = 2;
                msg_for_proc.index = msg_from_proc.index;
                msg_for_proc.page = -2;

                msgsnd(msgid_pmmu, &msg_for_proc, sizeof(struct msg_pmmu) - sizeof(long), 0);
                printf("mmu: invalid reference sent to process\n");
                fflush(stdout);

                //inform scheduler to remove process and schedule next process
                printf("TRYING TO ACCESS INVALID PAGE REFERENCE\n");
                fflush(stdout);

                //send type 2 message to scheduler
                msg_for_sched.msg_type = 2;
                msg_for_sched.index = msg_from_proc.index;

                msgsnd(msgid_smmu, &msg_for_sched, sizeof(struct message) - sizeof(long), 0);
                printf("Process %d terminated\n", msg_from_proc.index);
                fflush(stdout);

            }else{
                //check if page is in page table
                //start from page number c_vm[index] and go till c_vm[index+1] - 1
                if(pt[c_vm[msg_from_proc.index] + msg_from_proc.page].valid==1 && pt[c_vm[msg_from_proc.index] + msg_from_proc.page].frame != -1){
                    //page is in memory
                    
                    // //update time of last access of all pages in page table
                    // int sum = c_vm[msg_from_proc.index+1] + vm[msg_from_proc.index];
                    // for(int i = 0; i < sum; i++){
                    //     if(i == c_vm[msg_from_proc.index] + msg_from_proc.page){
                    //         pt[i].time = 0;
                    //     }else{
                    //         if(pt[i].valid == 1){
                    //             if(pt[i].time == BIG_NUM){

                    //             }else{
                    //                 pt[i].time++;
                    //             }
                    //         }
                    //     }
                    // }

                    int frame = pt[c_vm[msg_from_proc.index] + msg_from_proc.page].frame;
                    msg_for_proc.msg_type = 2;
                    msg_for_proc.index = msg_from_proc.index;
                    msg_for_proc.page = frame;

                    msgsnd(msgid_pmmu, &msg_for_proc, sizeof(struct msg_pmmu) - sizeof(long), 0);
                    printf("mmu: page found: %d\n", frame);
                    fflush(stdout);

                    //send type 1 message to scheduler
                    msg_for_sched.msg_type = 1;
                    msg_for_sched.index = msg_from_proc.index;

                    msgsnd(msgid_smmu, &msg_for_sched, sizeof(struct message) - sizeof(long), 0);
                    printf("mmu: page found sent to scheduler\n");

                }else{
                    //page fault
                    //send -1 to process
                    msg_for_proc.msg_type = 2;
                    msg_for_proc.index = msg_from_proc.index;
                    msg_for_proc.page = -1;

                    msgsnd(msgid_pmmu, &msg_for_proc, sizeof(struct msg_pmmu) - sizeof(long), 0);
                    printf("mmu: page fault sent to process\n");
                    fflush(stdout);

                    //check for free frame
                    int frame = -1;
                    for(int i = 0; i < f; i++){
                        if(freeframe_list[i]==1){
                            frame = i;
                            break;
                        }
                    }
                    printf("mmu page fault: frame found: %d\n", frame);
                    fflush(stdout);

                    if(frame == -1){
                        //do LRU replacement
                        int max_index = c_vm[msg_from_proc.index];
                        for(int i = c_vm[msg_from_proc.index]; i < c_vm[msg_from_proc.index+1]; i++){
                            if(pt[i].time > pt[max_index].time){
                                max_index = i;
                            }
                        }

                        //update page table
                        frame = pt[max_index].frame;

                        pt[max_index].frame = -1;
                        pt[max_index].valid = 0;
                        pt[max_index].time = BIG_NUM;

                        pt[c_vm[msg_from_proc.index] + msg_from_proc.page].frame = frame;
                        pt[c_vm[msg_from_proc.index] + msg_from_proc.page].valid = 1;

                    }else{
                        //update page table and free frame list
                        pt[c_vm[msg_from_proc.index] + msg_from_proc.page].frame = frame;
                        pt[c_vm[msg_from_proc.index] + msg_from_proc.page].valid = 1;

                        freeframe_list[frame] = 0;

                        printf("mmu page fault: frame updated: %d\n", frame);
                        fflush(stdout);
                    }

                    //update time of last access of all pages in page table
                    // int sum = c_vm[msg_from_proc.index+1] + vm[msg_from_proc.index];
                    // printf("sum, c_vm, page, idx: %d %d %d %d\n", sum, c_vm[msg_from_proc.index], vm[msg_from_proc.index], msg_for_proc.index);
                    // fflush(stdout);
                    // for(int i = 0; i < sum; i++){
                    //     if(i == c_vm[msg_from_proc.index] + msg_from_proc.page){
                    //         pt[i].time = 0;
                    //     }else{
                    //         if(pt[i].valid == 1){
                    //             if(pt[i].time == BIG_NUM){

                    //             }else{
                    //                 pt[i].time++;
                    //             }
                    //         }
                    //     }
                    // }

                    //send type 1 message to scheduler
                    msg_for_sched.msg_type = 1;
                    msg_for_sched.index = msg_from_proc.index;

                    msgsnd(msgid_smmu, &msg_for_sched, sizeof(struct message) - sizeof(long), 0);
                    printf("mmu: page fault sent to scheduler\n");
                    fflush(stdout);
                }
            }            
        }
    }    
}