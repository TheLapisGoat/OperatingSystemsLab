#include <stdio.h>
#include <pthread.h>

int done = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;

void thr_exit() {
    pthread_mutex_lock(&m);
    done = 1;
    pthread_cond_signal(&c);
    pthread_mutex_unlock(&m);
}

void * child(void *arg) {
    printf("child\n");
    thr_exit();
    return NULL;
}

void * thr_join() {
    pthread_mutex_lock(&m);
    while (!done) pthread_cond_wait(&c, &m);
    pthread_mutex_unlock(&m);
    return NULL;
}

int main() {
    pthread_t p;
    pthread_create(&p, NULL, child, NULL);
    thr_join();
    printf("parent: end\n");
    return 0;
}