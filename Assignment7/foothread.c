#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "foothread.h"

struct foothread_table ft_table[FOOTHREAD_THREADS_MAX];
int ft_table_index = 0;
int table_sem_key = SEM_TABLE_KEY;
int exit_sem_key = SEM_EXIT_KEY;
int leader_exit_sem_key = SEM_LEADER_EXIT_KEY;
int active_joinable_threads = 0;
int total_threads = 0;

void foothread_attr_setjointype (foothread_attr_t * foo_attr, int detach_state) {
    foo_attr->detach_state = detach_state;
}

void foothread_attr_setstacksize (foothread_attr_t * foo_attr, int stack_size) {
    foo_attr->stack_size = stack_size;
}

struct sembuf p_op = { 0, -1, SEM_UNDO };
struct sembuf v_op = { 0, 1, SEM_UNDO };

foothread_mutex_t foo_mutex;                                //Will be used for mutual exclusion
foothread_barrier_t foo_barriers[FOOTHREAD_THREADS_MAX];    //Will be used for waiting to exit
foothread_barrier_t foo_leader_barrier;                     //Will be used for waiting by the leader

void foothread_create(foothread_t * foothread, foothread_attr_t * foo_attr, int (*start_routine)(void *), void * arg) {

    //Check if the thread is the first thread
    if (ft_table_index == 0) {
        active_joinable_threads = 0;
        total_threads = 1;
        //Initializing the mutex
        foothread_mutex_init(&foo_mutex);

        //Initializing all the barriers
        for (int i = 0; i < FOOTHREAD_THREADS_MAX; i++) {
            foothread_barrier_init(&foo_barriers[i], 2);
        }

        //Initializing the leader barrier
        foothread_barrier_init(&foo_leader_barrier, 2);
    }

    //Waiting on the mutex
    foothread_mutex_lock(&foo_mutex);

    //Check if foo_attr is NULL
    foothread_attr_t temp_attr = FOOTHREAD_ATTR_INITIALIZER;
    if (foo_attr != NULL) {
        temp_attr = *foo_attr;
    }
    //Allocate stack
    void * stack = malloc(temp_attr.stack_size);
    
    //Creating child thread
    ft_table[ft_table_index].status = temp_attr.detach_state;
    ft_table[ft_table_index].stack = stack;
    ft_table[ft_table_index].id = clone(start_routine, stack + temp_attr.stack_size, CLONE_THREAD | CLONE_SIGHAND | CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_IO, arg);
    //Set the id of the thread
    if (foothread != NULL) {
        foothread->id = ft_table[ft_table_index].id;
    }
    ft_table_index++;
    total_threads++;

    if (temp_attr.detach_state == FOOTHREAD_JOINABLE) {
        active_joinable_threads++;
    }

    //Release the mutex
    foothread_mutex_unlock(&foo_mutex);
}

void foothread_exit() {
    //Waiting on the mutex
    foothread_mutex_lock(&foo_mutex);

    //Check if the thread is the leader
    if (gettid() == getpid()) {
        //Check if there are any active joinable threads
        if (active_joinable_threads == 0) {
            //Releasing all the barriers
            for (int i = 0; i < ft_table_index; i++) {
                //Checking if thread is joinable
                if (ft_table[i].status == FOOTHREAD_JOINABLE) {
                    foothread_barrier_wait(&foo_barriers[i]);
                }
            }
            total_threads--;
            //Releasing the mutex
            foothread_mutex_unlock(&foo_mutex);

            if (total_threads == 0) {
                foothread_mutex_destroy(&foo_mutex);
                for (int i = 0; i < FOOTHREAD_THREADS_MAX; i++) {
                    foothread_barrier_destroy(&foo_barriers[i]);
                }
                foothread_barrier_destroy(&foo_leader_barrier);
            }

            return;
        }

        //Release the mutex
        foothread_mutex_unlock(&foo_mutex);

        //Waiting on the leader barrier
        foothread_barrier_wait(&foo_leader_barrier);

        //Releasing all the barriers
        for (int i = 0; i < ft_table_index; i++) {
            //Checking if thread is joinable
            if (ft_table[i].status == FOOTHREAD_JOINABLE) {
                foothread_barrier_wait(&foo_barriers[i]);
            }
        }

        foothread_mutex_lock(&foo_mutex);
        total_threads--;
        foothread_mutex_unlock(&foo_mutex);

        if (total_threads == 0) {
            foothread_mutex_destroy(&foo_mutex);
            for (int i = 0; i < FOOTHREAD_THREADS_MAX; i++) {
                foothread_barrier_destroy(&foo_barriers[i]);
            }
            foothread_barrier_destroy(&foo_leader_barrier);
        }

        return;
    }

    //Thread is follower
    if (ft_table[ft_table_index - 1].status == FOOTHREAD_JOINABLE) {
        active_joinable_threads--;
    } else {
        //Release the mutex
        total_threads--;
        foothread_mutex_unlock(&foo_mutex);

        if (total_threads == 0) {
            foothread_mutex_destroy(&foo_mutex);
            for (int i = 0; i < FOOTHREAD_THREADS_MAX; i++) {
                foothread_barrier_destroy(&foo_barriers[i]);
            }
            foothread_barrier_destroy(&foo_leader_barrier);
        }
        exit(0);
    }

    //If active joinable threads is 0, releasing the leader barrier
    if (active_joinable_threads == 0) {
        foothread_barrier_wait(&foo_leader_barrier);
    }

    //Release the mutex
    foothread_mutex_unlock(&foo_mutex);

    //Wait on the corresponding barrier
    int current_tid = gettid();
    int i = 0;
    for (i = 0; i < ft_table_index; i++) {
        if (ft_table[i].id == current_tid) {
            break;
        }
    }
    foothread_barrier_wait(&foo_barriers[i]);

    foothread_mutex_lock(&foo_mutex);
    total_threads--;
    foothread_mutex_unlock(&foo_mutex);

    if (total_threads == 0) {
        foothread_mutex_destroy(&foo_mutex);
        for (int i = 0; i < FOOTHREAD_THREADS_MAX; i++) {
            foothread_barrier_destroy(&foo_barriers[i]);
        }
        foothread_barrier_destroy(&foo_leader_barrier);
    }
    //Exit the thread
    exit(0);
}

void foothread_mutex_init (foothread_mutex_t * mutex) {
    //Creating a semaphore
    int sem_id = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }
    mutex->sem_id = sem_id;
    
    //Setting the value of the semaphore to 1
    if (semctl(sem_id, 0, SETVAL, 1) < 0) {
        perror("semctl");
        exit(1);
    }

    mutex->owner_id = -1;
}

void foothread_mutex_lock (foothread_mutex_t * mutex) {
    //Wait on the semaphore
    if (semop(mutex->sem_id, &p_op, 1) < 0) {
        perror("semop");
        exit(1);
    }
    mutex->owner_id = gettid();
}

void foothread_mutex_unlock (foothread_mutex_t * mutex) {
    //Check if mutex is locked
    if (mutex->owner_id == -1) {
        perror("Error: Mutex is already unlocked");
        exit(1);
    }
    //Check if the thread is the owner of the mutex
    if (mutex->owner_id != gettid()) {
        perror("Error: Mutex can only be unlocked by the owner");
        exit(1);
    }
    //Release the semaphore
    mutex->owner_id = -1;
    if (semop(mutex->sem_id, &v_op, 1) < 0) {
        perror("semop");
        exit(1);
    }
}

void foothread_mutex_destroy (foothread_mutex_t * mutex) {
    //Remove the semaphore
    if (semctl(mutex->sem_id, 0, IPC_RMID) < 0) {
        perror("semctl");
        exit(1);
    }
}

void foothread_barrier_init (foothread_barrier_t * barrier, int count) {
    //Creating a semaphore for waiting for all threads to reach barrier
    int sem_id = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }
    barrier->wait_sem_id = sem_id;
    
    //Setting the value of the semaphore to 0
    if (semctl(sem_id, 0, SETVAL, 0) < 0) {
        perror("semctl");
        exit(1);
    }

    //Creating a semaphore for mutex of count
    sem_id = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }
    barrier->mtx_sem_id = sem_id;

    //Setting the value of the semaphore to 1
    if (semctl(sem_id, 0, SETVAL, 1) < 0) {
        perror("semctl");
        exit(1);
    }

    barrier->waiting_count = 0;
    barrier->max_count = count;
}

void foothread_barrier_wait (foothread_barrier_t * barrier) {
    //Wait on the mutex semaphore
    if (semop(barrier->mtx_sem_id, &p_op, 1) < 0) {
        perror("semop");
        exit(1);
    }
    barrier->waiting_count++;
    if (barrier->waiting_count == barrier->max_count) {
        //Release the wait semaphore
        struct sembuf v_op_temp = {0, barrier->max_count - 1, SEM_UNDO};
        if (semop(barrier->wait_sem_id, &v_op_temp, 1) < 0) {
            perror("semop");
            exit(1);
        }
        //Resetting the barrier
        barrier->waiting_count = 0;
        //Release the mutex semaphore
        if (semop(barrier->mtx_sem_id, &v_op, 1) < 0) {
            perror("semop");
            exit(1);
        }
        return;
    }
    //Release the mutex semaphore
    if (semop(barrier->mtx_sem_id, &v_op, 1) < 0) {
        perror("semop");
        exit(1);
    }
    //Wait on the wait semaphore
    if (semop(barrier->wait_sem_id, &p_op, 1) < 0) {
        perror("semop");
        exit(1);
    }
}

void foothread_barrier_destroy (foothread_barrier_t * barrier) {
    //Remove the wait semaphore
    if (semctl(barrier->wait_sem_id, 0, IPC_RMID) < 0) {
        perror("semctl");
        exit(1);
    }
    //Remove the mutex semaphore
    if (semctl(barrier->mtx_sem_id, 0, IPC_RMID) < 0) {
        perror("semctl");
        exit(1);
    }
}