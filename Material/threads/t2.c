#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

void * first() {
    int i;
    for (i = 0;; i++) {
        printf("First thread: %d\n", i);
        sleep(1);
    }
}

int main() {
    pthread_t th;
    int i;
    pthread_create(&th, NULL, (void *) &first, NULL);
    
    for (i = 0;; i++) {
        printf("Main thread: %d\n", i);
        sleep(1);
    }

    pthread_join(th, NULL);
    return 0;
}