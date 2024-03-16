#ifndef FOO_THREAD_H
#define FOO_THREAD_H
#include <stdio.h>
#include <stdlib.h>

#define FOOTHREAD_THREADS_MAX 100
#define FOOTHREAD_DEFAULT_STACK_SIZE 2097152
#define FOOTHREAD_JOINABLE 0
#define FOOTHREAD_DETACHED 1
#define FOOTHREAD_ATTR_INITIALIZER {FOOTHREAD_DEFAULT_STACK_SIZE, FOOTHREAD_JOINABLE}
#define SEM_TABLE_KEY 1234;
#define SEM_EXIT_KEY 1235;
#define SEM_LEADER_EXIT_KEY 1236;

typedef struct foothread_t {
    int id;
} foothread_t;

struct foothread_table {
    int id;
    int status;     //0: joinable, 1: detached
    void *stack;
};

typedef struct foothread_attr_t {
    int stack_size;
    int detach_state;
} foothread_attr_t;

void foothread_create(foothread_t * ,foothread_attr_t * ,int (*)(void *) , void *);

void foothread_attr_setjointype (foothread_attr_t * , int);
void foothread_attr_setstacksize (foothread_attr_t * , int);

void foothread_exit ();

typedef struct foothread_mutex_t {
    int sem_id;
    int owner_id;
} foothread_mutex_t;

typedef struct foothread_barrier_t {
    int wait_sem_id;    //semaphore for waiting for all threads to reach barrier
    int mtx_sem_id;     //semaphore for mutex of count
    int waiting_count;  //count of threads that are waiting at barrier
    int max_count;      //total number of threads that need to reach barrier
} foothread_barrier_t;

void foothread_mutex_init (foothread_mutex_t *);
void foothread_mutex_lock (foothread_mutex_t *);
void foothread_mutex_unlock (foothread_mutex_t *);
void foothread_mutex_destroy (foothread_mutex_t *);

void foothread_barrier_init (foothread_barrier_t *, int);
void foothread_barrier_wait (foothread_barrier_t *);
void foothread_barrier_destroy (foothread_barrier_t *);

#endif

