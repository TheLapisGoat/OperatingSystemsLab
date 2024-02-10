#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char * argv[]) {
    int n = atoi(argv[1]);  //Number of node
    int cur_node = atoi(argv[2]);  //Current node

    //Creating a key for shared memory A (adj matrix)
    key_t keyA = ftok("./graph.txt", 13);

    //Creating a key for shared memory idx (index of the node)
    key_t keyIdx = ftok("./graph.txt", 14);

    //Creating a key for the shared array T (topo sort array)
    key_t keyT = ftok("./graph.txt", 15);

    //Creating a key for the semaphore mtx
    key_t keyMtx = ftok("./graph.txt", 16);

    //Creating a key for the semaphore set sync
    key_t keySync = ftok("./graph.txt", 17);

    //Creating a key for the semaphore ntfy (notify)
    key_t keyNtfy = ftok("./graph.txt", 18);

    //Waiting for parents of the current node to finish (This will occur when the value of the cur_nodeth semaphore in the set sync is n)
    int semidSync = semget(keySync, n, 0666);
    struct sembuf waitSync;
    waitSync.sem_num = cur_node;
    waitSync.sem_op = -n;
    waitSync.sem_flg = 0;
    semop(semidSync, &waitSync, 1);

    //Waiting for mutex
    int semidMtx = semget(keyMtx, 1, 0666);
    struct sembuf waitMtx;
    waitMtx.sem_num = 0;
    waitMtx.sem_op = -1;
    waitMtx.sem_flg = 0;
    semop(semidMtx, &waitMtx, 1);

    //Attaching the shared memory for idx
    int shmidIdx = shmget(keyIdx, sizeof(int), 0666);
    int *idx = (int *) shmat(shmidIdx, NULL, 0);

    //Attaching the shared memory for T
    int shmidT = shmget(keyT, n * sizeof(int), 0666);
    int *T = (int *) shmat(shmidT, NULL, 0);

    //Adding the current node to the topological sort array
    T[*idx] = cur_node;
    (*idx)++;

    //Detaching the shared memory for idx
    shmdt(idx);

    //Detaching the shared memory for T
    shmdt(T);

    //Signalling the mutex
    struct sembuf signalMtx;
    signalMtx.sem_num = 0;
    signalMtx.sem_op = 1;
    signalMtx.sem_flg = 0;
    semop(semidMtx, &signalMtx, 1);

    //Attaching the shared memory for A
    int shmidA = shmget(keyA, n*n*sizeof(int), 0666);
    int *A = (int *) shmat(shmidA, NULL, 0);

    //Updating the indegree of the nodes that are the neighbors of the current node
    for (int i = 0; i < n; i++) {
        if (A[cur_node * n + i] == 1) {
            struct sembuf signalSync;
            signalSync.sem_num = i;
            signalSync.sem_op = 1;
            signalSync.sem_flg = 0;
            semop(semidSync, &signalSync, 1);
        }
    }

    //Detaching the shared memory for A
    shmdt(A);

    //Signalling the ntfy semaphore
    int semidNtfy = semget(keyNtfy, 1, 0666);
    struct sembuf signalNtfy;
    signalNtfy.sem_num = 0;
    signalNtfy.sem_op = 1;
    signalNtfy.sem_flg = 0;
    semop(semidNtfy, &signalNtfy, 1);

    return 0;
}