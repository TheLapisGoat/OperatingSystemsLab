#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define KRED  "\x1B[31m"
#define KBLU  "\x1B[36m"
#define KWHT  "\x1B[37m"

int SLEEPING;
int SESSION;
int ONGOING_SESSION;
int ASSIST_SLEEP;
long TOTAL_PATIENTS, TOTAL_REPORTER, TOTAL_SALES;
int CUR_PATIENTS, CUR_REPORTER, CUR_SALES;
pthread_mutex_t doc_sleep_mutex;
pthread_cond_t doc_sleep_cond;
int cur_time;
int SLEEP_PAT, SLEEP_REP, SLEEP_SAL;

event P_args[25];
event R_args[20];
event S_args[3];

pthread_mutex_t patient_mutex;
pthread_mutex_t reporter_mutex;
pthread_mutex_t sales_mutex;
pthread_cond_t patient_cond;
pthread_cond_t reporter_cond;
pthread_cond_t sales_cond;

pthread_mutex_t session_mutex;
pthread_cond_t session_cond;

pthread_cond_t assist_sleep_cond;

//Function that takes int time and print the time in 12 hour format (HH:MM AM/PM)
void printTime(int time) {
    time += 540;
    int hour = time / 60;
    int minute = time % 60;
    char *ampm = "am";
    if (hour >= 12) {
        ampm = "pm";
    }
    if (hour > 12) {
        hour -= 12;
    }
    printf("%02d:%02d%s", hour, minute, ampm);
}

void printTimeSingle(int time) {
    printf("[");
    printTime(time);
    printf("] ");
}

void printTimeRange(int start, int end) {
    printf("[");
    printTime(start);
    printf(" - ");
    printTime(end);
    printf("] ");
}

void * doctor(void *) {
    while (1) {
        //Waiting to be woken up by the assistant
        pthread_mutex_lock(&doc_sleep_mutex);
        while (SLEEPING) {
            pthread_cond_wait(&doc_sleep_cond, &doc_sleep_mutex);
        }
        SLEEPING = 1;

        //Check if quota is reached
        pthread_mutex_lock(&patient_mutex);
        pthread_mutex_lock(&reporter_mutex);
        pthread_mutex_lock(&sales_mutex);
        if (TOTAL_PATIENTS >= 25 && TOTAL_SALES >= 3 && CUR_PATIENTS == 0 && CUR_SALES == 0 && CUR_REPORTER == 0) {
            SESSION = 2;

            printf("%s", KRED);
            printTimeSingle(cur_time);
            printf("Doctor leaves\n");
            printf("%s", KWHT);

            pthread_mutex_unlock(&patient_mutex);
            pthread_mutex_unlock(&reporter_mutex);
            pthread_mutex_unlock(&sales_mutex);

            //Wake up assistant
            ASSIST_SLEEP = 0;
            pthread_cond_signal(&assist_sleep_cond);
            pthread_mutex_unlock(&doc_sleep_mutex);

            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&patient_mutex);
        pthread_mutex_unlock(&reporter_mutex);
        pthread_mutex_unlock(&sales_mutex);

        ONGOING_SESSION = 1;
        
        printf("%s", KRED);
        printTimeSingle(cur_time);
        printf("Doctor has next visitor\n");
        printf("%s", KWHT);
        

        //Wake up the appropriate thread
        pthread_mutex_lock(&reporter_mutex);
        pthread_mutex_lock(&sales_mutex);
        pthread_mutex_lock(&patient_mutex);
        if (CUR_REPORTER > 0) {
            SLEEP_REP = 0;
            pthread_cond_signal(&reporter_cond);
            CUR_REPORTER--;
        } else if (CUR_PATIENTS > 0) {
            SLEEP_PAT = 0;
            pthread_cond_signal(&patient_cond);
            CUR_PATIENTS--;
        } else if (CUR_SALES > 0) {
            SLEEP_SAL = 0;
            pthread_cond_signal(&sales_cond);
            CUR_SALES--;
        }
        pthread_mutex_unlock(&patient_mutex);
        pthread_mutex_unlock(&sales_mutex);
        pthread_mutex_unlock(&reporter_mutex);

        //Wait for the thread to finish
        pthread_mutex_lock(&session_mutex);
        while (ONGOING_SESSION) {
            pthread_cond_wait(&session_cond, &session_mutex);
        }

        pthread_mutex_unlock(&session_mutex);

        //Waking up the assistant
        ASSIST_SLEEP = 0;
        pthread_cond_signal(&assist_sleep_cond);
        pthread_mutex_unlock(&doc_sleep_mutex);
    }
}

void * patient(void * arg) {
    long pat_id = (long) arg;

    //Getting the mutex for the patient
    pthread_mutex_lock(&patient_mutex);
    //Wait for the doctor to be available
    while (SLEEP_PAT) {
        pthread_cond_wait(&patient_cond, &patient_mutex);
    }
    SLEEP_PAT = 1;

    cur_time += P_args[pat_id].duration;
    if (cur_time < P_args[pat_id].time + P_args[pat_id].duration) {
        cur_time = P_args[pat_id].time + P_args[pat_id].duration;
    }

    printf("%s", KBLU);
    printTimeRange(cur_time - P_args[pat_id].duration, cur_time);
    printf("Patient %ld is in doctor's chamber\n", pat_id + 1);
    printf("%s", KWHT);

    //Signal the doctor that the session is over
    pthread_mutex_lock(&session_mutex);
    ONGOING_SESSION = 0;
    pthread_cond_signal(&session_cond);
    pthread_mutex_unlock(&session_mutex);
    pthread_mutex_unlock(&patient_mutex);

    pthread_exit(NULL);
}

void * salesrep(void * arg) {
    long sales_id = (long) arg;

    //Getting the mutex for the sales
    pthread_mutex_lock(&sales_mutex);
    //Wait for the doctor to be available
    while (SLEEP_SAL) {
        pthread_cond_wait(&sales_cond, &sales_mutex);
    }
    SLEEP_SAL = 1;
    
    cur_time += S_args[sales_id].duration;
    if (cur_time < S_args[sales_id].time + S_args[sales_id].duration) {
        cur_time = S_args[sales_id].time + S_args[sales_id].duration;
    }

    printf("%s", KBLU);
    printTimeRange(cur_time - S_args[sales_id].duration, cur_time);
    printf("Sales representative %ld is in doctor's chamber\n", sales_id + 1);
    printf("%s", KWHT);

    //Signal the doctor that the session is over
    pthread_mutex_lock(&session_mutex);
    ONGOING_SESSION = 0;
    pthread_cond_signal(&session_cond);
    pthread_mutex_unlock(&session_mutex);
    pthread_mutex_unlock(&sales_mutex);

    pthread_exit(NULL);
}

void * reporter(void * arg) {
    long rep_id = (long) arg;

    //Getting the mutex for the reporter
    pthread_mutex_lock(&reporter_mutex);
    //Wait for the doctor to be available
    while (SLEEP_REP) {
        pthread_cond_wait(&reporter_cond, &reporter_mutex);
    }
    SLEEP_REP = 1;

    cur_time += R_args[rep_id].duration;
    if (cur_time < R_args[rep_id].time + R_args[rep_id].duration) {
        cur_time = R_args[rep_id].time + R_args[rep_id].duration;
    }
    
    printf("%s", KBLU);
    printTimeRange(cur_time - R_args[rep_id].duration, cur_time);
    printf("Reporter %ld is in doctor's chamber\n", rep_id + 1);
    printf("%s", KWHT);

    //Signal the doctor that the session is over
    pthread_mutex_lock(&session_mutex);
    ONGOING_SESSION = 0;
    pthread_cond_signal(&session_cond);
    pthread_mutex_unlock(&session_mutex);
    pthread_mutex_unlock(&reporter_mutex);

    pthread_exit(NULL);
}


int main() {
    //This is the assistant thread
    //Creating queue and initializing it with the file
    eventQ queue = initEQ("arrival.txt");

    pthread_t th_patient[25];
    pthread_t th_reporter[20];
    pthread_t th_sales[3];

    // //Printing all the events in the queue
    // int i = 0;
    // while (i < 100) {
    //     event ev = nextevent(queue);
    //     i++;
    //     if (ev.type == 'E') {
    //         break;
    //     } else {
    //         printf("Event: %c, Time: %d, Duration: %d\n", ev.type, ev.time, ev.duration);
    //         queue = delevent(queue);
    //     }
    // }

    //Initializing variables
    cur_time = 0;
    SLEEPING = 1;
    SESSION = 0;
    TOTAL_PATIENTS = 0;
    TOTAL_REPORTER = 0;
    TOTAL_SALES = 0;
    CUR_PATIENTS = 0;
    CUR_REPORTER = 0;
    CUR_SALES = 0;
    ASSIST_SLEEP = 0;
    pthread_mutex_init(&doc_sleep_mutex, NULL);
    pthread_cond_init(&doc_sleep_cond, NULL);

    pthread_mutex_init(&patient_mutex, NULL);
    pthread_mutex_init(&reporter_mutex, NULL);
    pthread_mutex_init(&sales_mutex, NULL);
    pthread_cond_init(&patient_cond, NULL);
    pthread_cond_init(&reporter_cond, NULL);
    pthread_cond_init(&sales_cond, NULL);

    SLEEP_PAT = 1;
    SLEEP_REP = 1;
    SLEEP_SAL = 1;

    pthread_mutex_init(&session_mutex, NULL);
    pthread_cond_init(&session_cond, NULL);

    ONGOING_SESSION = 0;

    pthread_cond_init(&assist_sleep_cond, NULL);

    //Creating the doctor thread
    pthread_t th_doc;
    pthread_create(&th_doc, NULL, doctor, NULL);

    //Going into the work loop
    while (1) {
        //Getting the mutex for the sleep condition
        pthread_mutex_lock(&doc_sleep_mutex);
        while (ASSIST_SLEEP) {
            pthread_cond_wait(&assist_sleep_cond, &doc_sleep_mutex);
        }
        ASSIST_SLEEP = 1;

        //If session has not started yet
        if (SESSION == 0) {
            //If the queue is empty, then the session can be started now
            if (emptyQ(queue) == 1) {
                SESSION = 1;
                pthread_mutex_unlock(&doc_sleep_mutex);
                ASSIST_SLEEP = 0;
                continue;
            } else {
                //If the queue is not empty, then get the next event
                event ev = nextevent(queue);
                //If the arrival of the event is before 9, then create a thread for the event
                if (ev.time < 0) {
                    //Deleting the event from the queue
                    queue = delevent(queue);
                    //Creating the thread for the event, if there is space
                    if (ev.type == 'P') {
                        if (TOTAL_PATIENTS < 25) {
                            //Creating a detached thread for the patient
                            pthread_attr_t attr;
                            pthread_attr_init(&attr);
                            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                            //Creating a copy of the event
                            P_args[TOTAL_PATIENTS] = ev;
                            //Passing id of the patient as the argument
                            pthread_create(&th_patient[TOTAL_PATIENTS], &attr, patient, (void *) TOTAL_PATIENTS);
                            TOTAL_PATIENTS++;
                            CUR_PATIENTS++;

                            printf("%-10s", "");
                            printTimeSingle(ev.time);
                            printf("Patient %ld arrives\n", TOTAL_PATIENTS);
                        } else {
                            //If there is no space, then delete the event
                            TOTAL_PATIENTS++;

                            printf("%-10s", "");
                            printTimeSingle(ev.time);
                            printf("Patient %ld arrives\n", TOTAL_PATIENTS);
                            printf("%-10s", "");
                            printTimeSingle(ev.time);
                            printf("Patient %ld leaves (quota full)\n", TOTAL_PATIENTS);
                        }
                    } else if (ev.type == 'S') {
                        if (TOTAL_SALES < 3) {
                            //Creating a detached thread for the sales
                            pthread_attr_t attr;
                            pthread_attr_init(&attr);
                            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                            //Creating a copy of the event
                            S_args[TOTAL_SALES] = ev;
                            //Passing id of the sales as the argument
                            pthread_create(&th_sales[TOTAL_SALES], &attr, salesrep, (void *) TOTAL_SALES);
                            TOTAL_SALES++;
                            CUR_SALES++;

                            printf("%-10s", "");
                            printTimeSingle(ev.time);
                            printf("Sales representative %ld arrives\n", TOTAL_SALES);
                        } else {
                            //If there is no space, then delete the event
                            TOTAL_SALES++;

                            printf("%-10s", "");
                            printTimeSingle(ev.time);
                            printf("Sales representative %ld arrives\n", TOTAL_SALES);
                            printf("%-10s", "");
                            printTimeSingle(ev.time);
                            printf("Sales representative %ld leaves (quota full)\n", TOTAL_SALES);
                        }
                    } else {
                        //Creating a detached thread for the reporter
                        pthread_attr_t attr;
                        pthread_attr_init(&attr);
                        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                        //Creating a copy of the event
                        R_args[TOTAL_REPORTER] = ev;
                        //Passing id of the reporter as the argument
                        pthread_create(&th_reporter[TOTAL_REPORTER], &attr, reporter, (void *) TOTAL_REPORTER);
                        TOTAL_REPORTER++;
                        CUR_REPORTER++;

                        printf("%-10s", "");
                        printTimeSingle(ev.time);
                        printf("Reporter %ld arrives\n", TOTAL_REPORTER);
                    }
                } else {
                    //If the arrival of the event is after 9, then the session has started
                    SESSION = 1;
                    cur_time = 0;
                }
                pthread_mutex_unlock(&doc_sleep_mutex);
                ASSIST_SLEEP = 0;
                continue;
            }
        }

        if (SESSION == 2) {
            //Go through all the remaining events
            while (emptyQ(queue) == 0) {
                event ev = nextevent(queue);
                queue = delevent(queue);
                if (ev.type == 'P') {
                    TOTAL_PATIENTS++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Patient %ld arrives\n", TOTAL_PATIENTS);
                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Patient %ld leaves (session over)\n", TOTAL_PATIENTS);
                } else if (ev.type == 'S') {
                    TOTAL_SALES++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Sales representative %ld arrives\n", TOTAL_SALES);
                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Sales representative %ld leaves (session over)\n", TOTAL_SALES);
                } else {
                    TOTAL_REPORTER++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Reporter %ld arrives\n", TOTAL_REPORTER);
                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Reporter %ld leaves (session over)\n", TOTAL_REPORTER);
                }
            }
            
            //Clean up everything
            pthread_mutex_unlock(&doc_sleep_mutex);
            pthread_mutex_destroy(&doc_sleep_mutex);
            pthread_cond_destroy(&doc_sleep_cond);
            pthread_mutex_destroy(&patient_mutex);
            pthread_mutex_destroy(&reporter_mutex);
            pthread_mutex_destroy(&sales_mutex);
            pthread_cond_destroy(&patient_cond);
            pthread_cond_destroy(&reporter_cond);
            pthread_cond_destroy(&sales_cond);
            pthread_mutex_destroy(&session_mutex);
            pthread_cond_destroy(&session_cond);
            pthread_cond_destroy(&assist_sleep_cond);
            exit(0);
        }

        //Check if queue is empty
        if (emptyQ(queue) == 1) {
            //If the queue is empty, then wake up the doctor
            SLEEPING = 0;
            pthread_cond_signal(&doc_sleep_cond);
            pthread_mutex_unlock(&doc_sleep_mutex);
            continue;
        }

        //Get the next event
        event ev = nextevent(queue);
        // printf("Current time: %d\n", cur_time);
        // printf("Next event: %c, Time: %d, Duration: %d\n", ev.type, ev.time, ev.duration);
        //Check if the event arrived before the current time
        if (ev.time <= cur_time) {
            //Delete the event from the queue
            queue = delevent(queue);
            //Create a thread for the event (if there is space)
            if (ev.type == 'P') {
                if (TOTAL_PATIENTS < 25) {
                    //Creating a detached thread for the patient
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                    //Creating a copy of the event
                    P_args[TOTAL_PATIENTS] = ev;
                    //Passing id of the patient as the argument
                    pthread_create(&th_patient[TOTAL_PATIENTS], &attr, patient, (void *) TOTAL_PATIENTS);
                    TOTAL_PATIENTS++;
                    CUR_PATIENTS++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Patient %ld arrives\n", TOTAL_PATIENTS);
                } else {
                    //If there is no space, then delete the event
                    TOTAL_PATIENTS++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Patient %ld arrives\n", TOTAL_PATIENTS);
                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Patient %ld leaves (quota full)\n", TOTAL_PATIENTS);
                }
            } else if (ev.type == 'S') {
                if (TOTAL_SALES < 3) {
                    //Creating a detached thread for the sales
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                    //Creating a copy of the event
                    S_args[TOTAL_SALES] = ev;
                    //Passing id of the sales as the argument
                    pthread_create(&th_sales[TOTAL_SALES], &attr, salesrep, (void *) TOTAL_SALES);
                    TOTAL_SALES++;
                    CUR_SALES++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Sales representative %ld arrives\n", TOTAL_SALES);
                } else {
                    //If there is no space, then delete the event
                    TOTAL_SALES++;

                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Sales representative %ld arrives\n", TOTAL_SALES);
                    printf("%-10s", "");
                    printTimeSingle(ev.time);
                    printf("Sales representative %ld leaves (quota full)\n", TOTAL_SALES);
                }
            } else {
                //Creating a detached thread for the reporter
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

                //Creating a copy of the event
                R_args[TOTAL_REPORTER] = ev;
                //Passing id of the reporter as the argument
                pthread_create(&th_reporter[TOTAL_REPORTER], &attr, reporter, (void *) TOTAL_REPORTER);
                TOTAL_REPORTER++;
                CUR_REPORTER++;

                printf("%-10s", "");
                printTimeSingle(ev.time);
                printf("Reporter %ld arrives\n", TOTAL_REPORTER);
            }
        } else {
            //If the event has not arrived yet, then check if there are any waiting threads (or the quota is full)
            if ((CUR_PATIENTS != 0 || CUR_REPORTER != 0 || CUR_SALES != 0) || (TOTAL_PATIENTS >= 25 && TOTAL_SALES >= 3)) {
                //If there are waiting threads, then wake up the doctor
                SLEEPING = 0;
                pthread_cond_signal(&doc_sleep_cond);
                pthread_mutex_unlock(&doc_sleep_mutex);
                continue;
            } else {
                //If there are no waiting threads, then increment the current time
                cur_time = ev.time;
                pthread_mutex_unlock(&doc_sleep_mutex);
                ASSIST_SLEEP = 0;
                continue;
            }
        }
        pthread_mutex_unlock(&doc_sleep_mutex);
        ASSIST_SLEEP = 0;
        continue;
    }
}