#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int mode = 0;       

int main(int argc, char *argv[]) {
    int mode;           //0: S, 1 : C, 2 : E
    int p_in, p_out;    //Pipe file descriptors
    int stdin_copy, stdout_copy;    //Copy of stdin and stdout
    int swap_p_in, swap_p_out;      //Swap pipe file descriptors

    if (argc == 1) {
        mode = 0;           //No arguments passed, so much be Supervisor process
    } else {
        mode = atoi(argv[1]);       //Argument passed, so must be C or E process
        p_in = atoi(argv[2]);       //Pipe input file descriptor
        p_out = atoi(argv[3]);      //Pipe output file descriptor
        swap_p_in = atoi(argv[4]);  //Swap pipe input file descriptor
        swap_p_out = atoi(argv[5]); //Swap pipe output file descriptor
    }

    if (mode == 0) {
        //Supervisor Mode
        printf("+++ CSE in supervisor mode: Started\n");

        //Create general communication pipe
        int p[2];
        pipe(p);
        printf("+++ CSE in supervisor mode: pfd = [%d %d]\n", p[0], p[1]);

        //Creating swap pipe
        int swap_p[2];
        pipe(swap_p);

        //Creating the child processes, starting with the C process
        printf("+++ CSE in supervisor mode: Forking first child in command-input mode\n");
        pid_t pid = fork();
        
        if (pid == 0) {
            //In First child, executing an xterm with pipe arguments and CSE in C mode
            char * pipe1 = malloc(10);
            char * pipe2 = malloc(10);
            char * pipe3 = malloc(10);
            char * pipe4 = malloc(10);
            sprintf(pipe1, "%d", p[0]);
            sprintf(pipe2, "%d", p[1]);
            sprintf(pipe3, "%d", swap_p[0]);
            sprintf(pipe4, "%d", swap_p[1]);
            execlp("xterm", "xterm", "-T", "First Child", "-e", "./CSE", "1", pipe1, pipe2, pipe3, pipe4, NULL);
        }

        //Creating the E process
        printf("+++ CSE in supervisor mode: Forking second child in execute mode\n");
        pid = fork();

        if (pid == 0) {
            //In Second child, executing an xterm with pipe arguments and CSE in E mode
            char * pipe1 = malloc(10);
            char * pipe2 = malloc(10);
            char * pipe3 = malloc(10);
            char * pipe4 = malloc(10);
            sprintf(pipe1, "%d", p[0]);
            sprintf(pipe2, "%d", p[1]);
            sprintf(pipe3, "%d", swap_p[0]);
            sprintf(pipe4, "%d", swap_p[1]);
            execlp("xterm", "xterm", "-T", "Second Child", "-e", "./CSE", "2", pipe1, pipe2, pipe3, pipe4, NULL);
        }
        //Wait for both children to finish
        wait(NULL);
        wait(NULL);
        //Exit
        exit(0);
    }

    char * command = malloc(100);

    if (mode == 2) {
        //Execute Mode
        //Making copy of stdin and redirecting stdin to pipe
        stdin_copy = dup(0);
        fprintf(stderr, "%d\n", stdin_copy);
        dup2(p_in, 0);
    } else if (mode == 1) {
        //Command Mode
        //Making copy of stdout and redirecting stdout to pipe
        stdout_copy = dup(1);
        fprintf(stderr, "%d\n", stdout_copy);
        dup2(p_out, 1);
    }


    while (1) {
        if (mode == 1) {
            //Command Mode
            memset(command, 0, 100);

            //Printing to stderr
            fprintf(stderr, "Enter Command> ");
            scanf("%s", command);

            //Checking if command is swaprole
            if (strcmp(command, "swaprole") == 0) {
                //Sending swaprole command to swap pipe
                write(swap_p_out, "swaprole", 9);

                //Changing mode to execute mode
                mode = 2;

                //Changing stdin to p_in and stdout to normal stdout
                dup2(p_in, 0);
                dup2(stdout_copy, 1);
                printf("+++ CSE in execute mode\n");
                fflush(stdout);

                continue;
            }
            //Writing command to stdout (pipe)
            printf("%s\n", command);
            fflush(stdout);
        } else if (mode == 2) {
            //Execute Mode
            memset(command, 0, 100);

            //Reading command from new stdin (pipe)
            printf("Waiting for command> ");
            fflush(stdout);

            scanf("%s", command);
            printf("%s\n", command);
            fflush(stdout);

            //Checking if command is swaprole
            if (strcmp(command, "swaprole") == 0) {
                //Changing mode to command mode
                mode = 1;

                //Changing stdin to normal stdin and stdout to p_out
                dup2(stdin_copy, 0);
                dup2(p_out, 1);

                continue;
            }

            //Executing command
            pid_t pid = fork();
            if (pid == 0) {
                execlp(command, command, NULL);
            } else {
                wait(NULL);
            }
        }
    }
}