#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include "foothread.h"

struct node {
    int parent;
    int value;
    int *children;
    int num_children;
};

int N;
struct node *tree;
foothread_mutex_t tree_mutex;
foothread_barrier_t * tree_barriers;
foothread_barrier_t start_computation;
foothread_barrier_t root_barrier;
foothread_t *tree_threads;

int t_main(void * arg) {
    long id = (long) arg;

    foothread_mutex_lock(&tree_mutex);

    // If leaf node, get input
    if (tree[id].num_children == 0) {
        printf("Leaf node %3ld :: Enter a positive integer: ", id);
        scanf("%d", &tree[id].value);
    }

    foothread_mutex_unlock(&tree_mutex);

    // Waiting on start_computation barrier
    foothread_barrier_wait(&start_computation);

    // Waiting on own barrier
    foothread_barrier_wait(&tree_barriers[id]);

    foothread_mutex_lock(&tree_mutex);

    // If not leaf node or root, print val
    if (tree[id].num_children != 0 && tree[id].parent != -1) {
        printf("Internal node %3ld gets the partial sum %d from its children\n", id, tree[id].value);
    }

    // Adding own value to parent's partial sum
    if (tree[id].parent != -1) {
        tree[tree[id].parent].value += tree[id].value;
    }

    // Unlocking mutex
    foothread_mutex_unlock(&tree_mutex);

    // Waiting on parents barrier
    if (tree[id].parent != -1) {
        foothread_barrier_wait(&tree_barriers[tree[id].parent]);
        foothread_barrier_wait(&root_barrier);
    } else {
        // If root, first wait on root_barrier
        foothread_barrier_wait(&root_barrier);
        printf("Sum at root (node %ld) = %d\n", id, tree[id].value);
    }

    // Exiting
    foothread_exit();

    return 0;
}

int main() {
    // Opening File
    FILE *fp = fopen("tree.txt", "r");

    // Reading number of nodes
    fscanf(fp, "%d", &N);

    // Allocating memory for tree
    tree = (struct node *) malloc (N * sizeof(struct node));
    memset(tree, 0, N * sizeof(struct node));

    // Reading tree
    for (int i = 0; i < N; i++) {
        int curr_node, par_node;
        fscanf(fp, "%d %d", &curr_node, &par_node);
        if (curr_node != par_node) {
            tree[curr_node].parent = par_node;
            tree[par_node].children = (int *) realloc (tree[par_node].children, (tree[par_node].num_children + 1) * sizeof(int));
            tree[par_node].children[tree[par_node].num_children] = curr_node;
            tree[par_node].num_children++;
        } else {
            tree[curr_node].parent = -1;
        }
    }

    // Closing file
    fclose(fp);

    // Initializing mutex
    foothread_mutex_init(&tree_mutex);

    // Allocating memory for barriers
    tree_barriers = (foothread_barrier_t *) malloc (N * sizeof(foothread_barrier_t));

    // Initializing barriers
    for (int i = 0; i < N; i++) {
        foothread_barrier_init(&tree_barriers[i], tree[i].num_children + 1);
    }
    foothread_barrier_init(&start_computation, N);
    foothread_barrier_init(&root_barrier, N);

    // Allocating memory for threads
    tree_threads = (foothread_t *) malloc (N * sizeof(foothread_t));

    // Creating threads
    for (long i = 0; i < N; i++) {
        foothread_create(&tree_threads[i], NULL, t_main, (void *) i);
    }

    // Exiting
    foothread_exit();

    // Freeing memory
    free(tree);
    free(tree_barriers);
    free(tree_threads);

    // Destroying mutex
    foothread_mutex_destroy(&tree_mutex);

    // Destroying barriers
    foothread_barrier_destroy(&start_computation);
    foothread_barrier_destroy(&root_barrier);
    for (int i = 0; i < N; i++) {
        foothread_barrier_destroy(&tree_barriers[i]);
    }

    return 0;
}