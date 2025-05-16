#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include "a2_helper.h"
#include <fcntl.h>

#define NO_THREADS_P7 4
#define NO_THREADS_P5 35
#define NO_MAX_THREADS 4
#define NO_THREADS_P4 6

typedef struct{
    int id;
    sem_t* sem_P7T2;
    sem_t* sem_P7T3;
}TH_STRUCT_P7;

typedef struct{
    int id;
    sem_t* sem_limit;
    sem_t* semT14;
    sem_t* sem_barrier;
    sem_t* sem_equal4;
    sem_t* sem_cnt;
    int* running_threads;
}TH_STRUCT_P5;

sem_t* sem_P4T2;
sem_t* sem_P7T4;

void* th_func_P7(void* arg){
    TH_STRUCT_P7* data = (TH_STRUCT_P7*)arg;
    int id = data->id;

    if(id == 2){
        info(BEGIN, 7, id);
        sem_post(data->sem_P7T2);
        sem_wait(data->sem_P7T3);
        info(END, 7, id);
    }
    else if(id == 3){
        sem_wait(data->sem_P7T2);
        info(BEGIN, 7, id);
        info(END, 7, id);
        sem_post(data->sem_P7T3);
    }
    else if(id == 4){
        sem_wait(sem_P4T2);
        info(BEGIN, 7, id);
        info(END, 7, id);
        sem_post(sem_P7T4);
    } 
    else{
        info(BEGIN, 7, id);
        info(END, 7, id);
    }
    return NULL;
}


void* th_func_P5(void* arg){
    TH_STRUCT_P5* data = (TH_STRUCT_P5*)arg;
    int id = data->id;

    if(id != 14){
        sem_wait(data->semT14);
        sem_post(data->semT14);
    }

    sem_wait(data->sem_limit);
    info(BEGIN, 5, id);

    sem_wait(data->sem_cnt);
    (*data->running_threads)++;
    if(*data->running_threads == 4){
        sem_post(data->sem_equal4);
    }
    sem_post(data->sem_cnt);

    if(id == 14){
        sem_post(data->semT14);
        sem_wait(data->sem_equal4);
    } else {
        sem_wait(data->sem_barrier);
        sem_post(data->sem_barrier);
    }

    info(END, 5, id);

    if(id == 14){
        sem_post(data->sem_barrier);
    }

    sem_post(data->sem_limit);
    return NULL;
}

void* th_func_P4(void* arg){
    int* id = (int*)arg;

    if (*id == 4) {
        sem_wait(sem_P7T4);
    }

    info(BEGIN, 4, *id);
    info(END, 4, *id);

    if (*id == 2) {
        sem_post(sem_P4T2);
    }

    return NULL;
}


int main(){
    init();

    sem_P4T2 = sem_open("P4T2", O_CREAT, 0644, 0);
    sem_P7T4 = sem_open("P7T4", O_CREAT, 0644, 0);

    info(BEGIN, 1, 0); // P1
    
    if(fork() == 0){ // P2
        info(BEGIN, 2, 0);
        if(fork() == 0){ // P3
            info(BEGIN, 3, 0);
            if(fork() == 0){ // P4
                info(BEGIN, 4, 0);
                pthread_t threads[NO_THREADS_P4];
                int data[NO_THREADS_P4];

                for(int i = 0; i < NO_THREADS_P4; i++){
                    data[i]= i + 1;
                    pthread_create(&threads[i], NULL, th_func_P4, &data[i]);
                }

                for(int i = 0; i < NO_THREADS_P4; i++){
                    pthread_join(threads[i], NULL);
                }

                if(fork() == 0){ // P6
                    info(BEGIN, 6, 0);
                    info(END, 6, 0);
                    exit(0);
                }
                wait(NULL);
                info(END, 4, 0);
                exit(0);
            }
            wait(NULL);
            info(END, 3, 0);
            exit(0);
        }
        if(fork() == 0){ // P8
            info(BEGIN, 8, 0);
            info(END, 8, 0);
            exit(0);
        }
        wait(NULL);
        wait(NULL);
        info(END, 2, 0);
        exit(0);
    }

    if(fork() == 0){ // P5
        info(BEGIN, 5, 0);
        pthread_t threads[NO_THREADS_P5];
        TH_STRUCT_P5 data[NO_THREADS_P5];

        sem_t sem_limit, sem_barrier, sem_cnt, sem_equal4, semT14;

        sem_init(&sem_limit, 0, NO_MAX_THREADS);
        sem_init(&sem_barrier, 0, 0);
        sem_init(&sem_cnt, 0, 1);
        sem_init(&sem_equal4, 0, 0);
        sem_init(&semT14, 0, 0);

        int running_threads = 0;

        for(int i = 0; i < NO_THREADS_P5; i++){
            data[i].id = i + 1;
            data[i].sem_limit = &sem_limit;
            data[i].sem_barrier = &sem_barrier;
            data[i].sem_cnt = &sem_cnt;
            data[i].sem_equal4 = &sem_equal4;
            data[i].semT14 = &semT14;
            data[i].running_threads = &running_threads;

            pthread_create(&threads[i], NULL, th_func_P5, &data[i]);
        }

        for(int i = 0; i < NO_THREADS_P5; i++){
            pthread_join(threads[i], NULL);
        }

        sem_destroy(&sem_limit);
        sem_destroy(&sem_barrier);
        sem_destroy(&sem_cnt);
        sem_destroy(&sem_equal4);
        sem_destroy(&semT14);

        info(END, 5, 0);
        exit(0);
    }

    if(fork() == 0){ // P7
        info(BEGIN, 7, 0);
        pthread_t threads[NO_THREADS_P7];
        TH_STRUCT_P7 data[NO_THREADS_P7];

        sem_t sem_P7T2, sem_P7T3;

        sem_init(&sem_P7T2, 0, 0);
        sem_init(&sem_P7T3, 0, 0);

        for(int i = 0; i < NO_THREADS_P7; i++){
            data[i].id = i + 1;
            data[i].sem_P7T3 = &sem_P7T3;
            data[i].sem_P7T2 = &sem_P7T2;
            pthread_create(&threads[i], NULL, th_func_P7, &data[i]);
        }

        for(int i = 0; i < NO_THREADS_P7; i++){
            pthread_join(threads[i], NULL);
        }

        sem_destroy(&sem_P7T3);
        sem_destroy(&sem_P7T2);

        info(END, 7, 0);
        exit(0);
    }

    if(fork() == 0){ // P9
        info(BEGIN, 9, 0);
        info(END, 9, 0);
        exit(0);
    }

    wait(NULL);
    wait(NULL);
    wait(NULL);
    wait(NULL);

    sem_unlink("P4T2");
    sem_unlink("P7T4");

    info(END, 1, 0);
    return 0;
}
