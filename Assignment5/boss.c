#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int n;  //Number of node

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

    //Opening graph.txt file to get the value of n
    FILE *file = fopen("graph.txt", "r");
    if (file == NULL) {
        printf("Error: File not found\n");
        exit(1);
    }
    fscanf(file, "%d", &n);

    //Creating a 2D array in shared memory to store the graph
    int shmidA = shmget(keyA, n*n*sizeof(int), IPC_CREAT | 0666);
    int *A = (int *) shmat(shmidA, NULL, 0);

    //Reading the graph from the file and storing it in shared memory
    for (int i = 0; i < n*n; i++) {
        fscanf(file, "%d", &A[i]);
    }

    //Closing the file
    fclose(file);

    //Creating a shared memory to store the index of the node
    int shmidIdx = shmget(keyIdx, sizeof(int), IPC_CREAT | 0666);
    int *idx = (int *) shmat(shmidIdx, NULL, 0);
    *idx = 0;

    //Creating a shared memory to store the topological sort array
    int shmidT = shmget(keyT, n * sizeof(int), IPC_CREAT | 0666);
    int *T = (int *) shmat(shmidT, NULL, 0);

    //Creating the semaphore mtx
    int semidMtx = semget(keyMtx, 1, 0666 | IPC_CREAT);
    //Initializing mtx
    semctl(semidMtx, 0, SETVAL, 1);

    //Creating the semaphore set sync (set of n semaphores)
    int semidSync = semget(keySync, n, 0666 | IPC_CREAT);

    //Setting the value of the semaphores in sync to n
    for (int i = 0; i < n; i++) {
        semctl(semidSync, i, SETVAL, n);
    }

    //Initializing the semaphores in sync to (n - indegree of the node)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            //Checking if there is an edge from i to j
            if (A[i*n + j] == 1) {
                //Getting the value of the jth semaphore
                int val = semctl(semidSync, j, GETVAL, 0);
                //Decrementing the value of the jth semaphore
                semctl(semidSync, j, SETVAL, val - 1);
            }
        }
    }

    // //Printing the value of the semaphores in sync
    // for (int i = 0; i < n; i++) {
    //     printf("%d ", semctl(semidSync, i, GETVAL, 0));
    // }

    //Creating the semaphore ntfy
    int semidNtfy = semget(keyNtfy, 1, 0666 | IPC_CREAT);
    //Initializing ntfy
    semctl(semidNtfy, 0, SETVAL, 0);

    printf("+++ Boss: Setup done...\n");

    //Waiting for the child processes to finish. This is done by waiting for the value of ntfy to become n
    struct sembuf opNtfy;
    opNtfy.sem_num = 0;
    opNtfy.sem_op = -n;
    opNtfy.sem_flg = 0;
    semop(semidNtfy, &opNtfy, 1);

    printf("+++ Topological sorting of the vertices\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", T[i]);
    }

    //Checking if the topological sort is correct
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (A[T[j]*n + T[i]] == 1) {
                printf("\n+++ Boss: Error: The topological sort is incorrect\n");
                return 1;
            }
        }
    }
    printf("\n+++ Boss: Well done, my team...");

    //Deleting the shared memory
    shmdt(A);
    shmctl(shmidA, IPC_RMID, NULL);
    shmdt(idx);
    shmctl(shmidIdx, IPC_RMID, NULL);
    shmdt(T);
    shmctl(shmidT, IPC_RMID, NULL);
    semctl(semidMtx, 0, IPC_RMID);
    semctl(semidSync, 0, IPC_RMID);
    semctl(semidNtfy, 0, IPC_RMID);
    return 0;
}