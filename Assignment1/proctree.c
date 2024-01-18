//Header files
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    //Get initial level
    int level = 0;
    if (argc > 2) {         //If level is given, set to that level, else keep it 0
        level = atoi(argv[2]);
    } else if (argc == 1) { //Ask for city name
        printf("Run with a node name\n");
        exit(0);
    }

    FILE *fp = fopen("treeinfo.txt", "r");          //Open file
    char buffer[64];                    //Buffer
    int check = 0;                      //Check if city is found
    while (fscanf(fp, "%s", buffer) == 1) {         //Scan file word by word
        if (strcmp(buffer, argv[1]) == 0) {
            check = 1;              //If word is found

            //Print current pid for city
            pid_t pid = getpid();
            for (int i = 0; i < level; i++) {           //Insert sufficient spaces for tree
                printf("    ");
            }
            printf("%s (%d)\n", argv[1], pid);   
            
            //Get number of children
            fscanf(fp, "%s", buffer);
            int children = atoi(buffer);
            
            //Fork to next child
            for (int i = 0; i < children; i++) {
                fscanf(fp, "%s", buffer);
                pid = fork();
                
                //If child
                if (pid == 0) {
                    //Buffer contains child city
                    char level_new[32];
                    sprintf(level_new, "%d", level + 1);
                    //Execute child process
                    execl("proctree", "./proctree", (const char *) buffer, (const char *) level_new, NULL);
                }
                //Wait for child
                wait(NULL);
            }
            break;
        } else {                //City not found
            fscanf(fp, "%s", buffer);               //Read rest of file
            int children = atoi(buffer);
            for (int i = 0; i < children; i++) {
                fscanf(fp, "%s", buffer);
            }
        }
    }
    
    //Check if city was not found
    if (!check) {
        printf("City %s not found", argv[1]);           
    }
    fclose(fp);     //Close file

    return 0;
}