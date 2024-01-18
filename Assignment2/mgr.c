//Header files
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>

int sus_jobs = 0;           //Count of suspended jobs
pid_t pid = -1;             //Process ID
int total_jobs = 1;         //Total jobs
int pid_arr[11];            //Array of process IDs
int pgid_arr[11];           //Array of process group IDs
char *status_arr[11];       //Array of statuses
char *name_arr[11];         //Array of names

void CtrlC() {          //Ctrl+C
    if (pid == -1) {                //No job running
        printf("mgr> ");
        return;
    }
    int child_it;                   //Child index
    for (int it = 0; it < total_jobs; it++) {
        if (pid_arr[it] == pid) {
            child_it = it; 
            break;
        }
    }
    pgid_arr[child_it] = getpgid(pid);              //Process group ID
    kill(pid, SIGINT);                              //Sending SIGINT
    strcpy(status_arr[child_it], "TERMINATED");     //Updating status
    printf("\n");                           
}

void CtrlZ() {         //Ctrl+Z
    if (pid == -1) {                //No job running
        printf("mgr> ");
        return;
    }
    int child_it;                   //Child index
    for (int it = 0; it < total_jobs; it++) {
        if (pid_arr[it] == pid) {
            child_it = it; 
            break;
        }
    }
    pgid_arr[child_it] = getpgid(pid);                  //Process group ID
    kill(pid, SIGTSTP);                         //Sending SIGTSTP
    strcpy(status_arr[child_it], "SUSPENDED");  //Updating status
    sus_jobs++;                 //Incrementing suspended jobs
    printf("\n");
}

int main() {
    char input;                                 //Input from user
    for (int i = 0; i < 11; i++) {              //Allocating memory for arrays
        status_arr[i] = (char *) malloc(20);
    }
    for (int i = 0; i < 11; i++) {
        name_arr[i] = (char *) malloc(20);
    }

    //Signal Handlers
    signal(SIGINT, CtrlC);                      //Ctrl+C
    signal(SIGTSTP, CtrlZ);                     //Ctrl+Z

    //Initial Values
    pid_arr[0] = getpid();                      //Process ID
    pgid_arr[0] = getpgrp();                    //Process Group ID
    strcpy(status_arr[0], "SELF");              //Status
    strcpy(name_arr[0], "mgr");                 //Name
    
    while (1) {
        printf("mgr> ");                        //Prompt
        scanf("%s", &input);                    //Read input

        if (input == 'h') {                     //Help
            printf("\tCOMMAND : ACTION\n\t   c    : Continue a suspended job\n\t   h    : Print this help message\n\t   k    : Kill a suspended job\n\t   p    : Print the process table\n\t   q    : Quit\n\t   r    : Run a new job\n");
        } else if (input == 'p') {              //Print process table
            printf("%-15s%-15s%-15s%-15s%-15s\n", "NO", "PID", "PGID", "STATUS", "NAME");
            for (int i = 0; i < total_jobs; i++) {
                printf("%-15d%-15d%-15d%-15s%-15s\n", i, pid_arr[i], pgid_arr[i], status_arr[i], name_arr[i]);
            }
        } else if (input == 'r') {              //Run a new job
            int status = 0;                     //Status of child
            if (total_jobs == 11) {             //Process table is full
                printf("Process table is full. Quiting...\n");
                //Killing all suspended jobs
                for (int i = 0; i < total_jobs; i++) {
                    if (strcmp(status_arr[i], "SUSPENDED") == 0) {
                        kill(pid_arr[i], SIGKILL);
                    }
                }
                exit(0);
            }
            
            char name = 'A' + rand() % 26;      //Name of job
            pid = fork();                       //Forking a new process

            if (pid == 0) {                     //In child
                setpgid(0, 0);                  //Setting process group ID
                execl("job", "./job", (const char *) &name, NULL);  //Executing job
            }

            // Update everything
            pid_arr[total_jobs] = pid;                      //Process ID
            strcpy(status_arr[total_jobs], "FINISHED");     //Status
            strcpy(name_arr[total_jobs], (const char *) &name);             //Name
            total_jobs++;                       //Incrementing total jobs

            // Stay in wait till child is finished, suspended or terminated
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

            pid = -1;                           //Resetting PID

        } else if (input == 'c') {
            if (sus_jobs == 0) {                //No suspended jobs
                continue;
            }
            int sus_count = 0;                  //Count of suspended jobs
            printf("Suspended jobs: ");
            for (int i = 0; i < total_jobs; i++) {                  //Printing all suspended jobs
                if (strcmp(status_arr[i], "SUSPENDED") == 0) {
                    printf("%d", i);
                    sus_count++;
                    if (sus_jobs == sus_count) {
                        printf(" ");
                    } else {
                        printf(", ");
                    }
                }
            }
            printf("(Pick one): ");
            int choice;                 //Choice of user
            int status = 0;             //Status of child
            scanf("%d", &choice);
            sus_jobs--;                         //Decrementing suspended jobs
            pid = pid_arr[choice];              //PID of chosen job
            kill(pid_arr[choice], SIGCONT);             //Continuing the job
            strcpy(status_arr[choice], "FINISHED");     //Updating status

            // Stay in wait till child is finished, suspended or terminated
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

            pid = -1;                           //Resetting PID

        } else if (input == 'k') {
            if (sus_jobs == 0) {                //No suspended jobs
                continue;
            }
            int sus_count = 0;                  //Count of suspended jobs
            printf("Suspended jobs: ");
            for (int i = 0; i < total_jobs; i++) {                  //Printing all suspended jobs
                if (strcmp(status_arr[i], "SUSPENDED") == 0) {
                    printf("%d", i);
                    sus_count++;
                    if (sus_jobs == sus_count) {
                        printf(" ");
                    } else {
                        printf(", ");
                    }
                }
            }
            printf("(Pick one): ");     
            int choice;                                     //Choice of user
            scanf("%d", &choice);
            sus_jobs--;                         //Decrementing suspended jobs
            kill(pid_arr[choice], SIGKILL);             //Killing the job
            strcpy(status_arr[choice], "KILLED");       //Updating status
        } else if (input == 'q') {
            //Killing all suspended jobs
            for (int i = 0; i < total_jobs; i++) {
                if (strcmp(status_arr[i], "SUSPENDED") == 0) {
                    kill(pid_arr[i], SIGKILL);
                }
            }
            //Exiting
            exit(0);
        }
    }
    return 0;
}