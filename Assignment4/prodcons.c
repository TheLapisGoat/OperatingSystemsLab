#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#ifdef VERBOSE
int verbose = 1;
#else
int verbose = 0;
#endif

#ifdef SLEEP
int flg_sleep = 1;
#else
int flg_sleep = 0;
#endif

int main(int argc, char * argv[]) {
    int shmid;
    pid_t pid;
    int * M;
    int n = 0, t = 0;

    //Reading n and m from user
    printf("n = "); 
    scanf("%d", &n);
    printf("t = ");
    scanf("%d", &t);

    //Creating shared memory
    shmid = shmget(IPC_PRIVATE, 2 * sizeof(int), IPC_CREAT | 0666);

    //Attaching shared memory
    M = (int *) shmat(shmid, NULL, 0);

    //Initializing shared memory
    M[0] = 0;
    M[1] = 0;

    int consumer_number;    //Consumer number, to keep track of which consumer the current process is
    //Creating producer and n consumer processes
    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid == 0) {
            consumer_number = i + 1;
            //Consumer process
            break;
        } else {
            //Producer process
            continue;
        }
    }

    if (pid == 0) {
        //Consumer process
        printf("%50sConsumer %d is alive\n", "", consumer_number);
        
        //Create the checksum
        int checksum = 0;
        
        //Creating count
        int count = 0;

        //Waiting for the producer to produce items, and consuming them if produced for this consumer
        while (M[0] != -1) {
            if (M[0] == consumer_number) {
                //Consuming the item
                checksum += M[1];

                //Incrementing the count
                count++;
                
                //Printing the item consumed
                if (verbose) {
                    printf("%50sConsumer %d reads %d\n", "", consumer_number, M[1]);
                }

                //Changing M[0] to 0
                M[0] = 0;
            }
        }

        //Printing the checksum
        printf("%50sConsumer %d has read %d items: Checksum = %d\n", "", consumer_number, count, checksum);

        //Detaching the shared memory
        shmdt(M);

        //Exiting the process
        exit(0);
    } else {
        //Producer process
        printf("Producer is alive\n");

        //Create the net checksum
        int * checksum = (int *) malloc(sizeof(int) * n);
        int * counts = (int *) malloc(sizeof(int) * n);
        for (int i = 0; i < n; i++) {
            checksum[i] = 0;
            counts[i] = 0;
        }

        //Looping t times, producing items randomly
        for (int i = 0; i < t; i++) {
            //Choosing a random consumer
            int c = rand() % n;

            //Producing a random 3 digit item
            int item = rand() % 1000;

            //Adding the item to the checksum
            checksum[c] += item;

            //Incrementing the count of the consumer
            counts[c]++;

            //Printing the item produced
            if (verbose) {
                printf("Producer produces %d for Consumer %d\n", item, c + 1);
            }

            //Changing M[0] to c;
            M[0] = c + 1;

            //Adding optional sleep
            if (flg_sleep) {
                usleep(5);
            }

            //Changing M[1] to item;
            M[1] = item;

            //Waiting for the consumer to consume the item
            while (M[0] != 0) {
                continue;
            }
        }

        //Changing M[0] to -1
        M[0] = -1;

        //Waiting for all the consumers to finish
        for (int i = 0; i < n; i++) {
            wait(NULL);
        }

        //Printing the checksum
        printf("Producer has produced %d items\n", t);
        for (int i = 0; i < n; i++) {
            printf("%d items for Consumer %d: Checksum = %d\n", counts[i], i + 1, checksum[i]);
        }

        //Detaching the shared memory
        shmdt(M);

        //Deleting the shared memory
        shmctl(shmid, IPC_RMID, NULL);
    }
    exit(0);
}