#ifndef WEB_H
#define WEB_H

#include <pthread.h> //pthread线程库

#define SERV "0.0.0.0"
#define QUEUE 20
#define BUFF_SIZE 1024
#define ISspace(x) isspace((int)(x))

typedef struct{
    pthread_t tid;
    long count;
} Thread;
Thread *tptr;

#endif
