#ifndef WEB_H
#define WEB_H

#include <pthread.h> //pthread线程库

#include <iostream>

#define SERV "0.0.0.0"
#define QUEUE 20
#define BUFF_SIZE 1024
#define ISspace(x) isspace((int)(x))

class Thread{
private:
    Thread(){}
    ~Thread(){}
    Thread(const Thread&){}
    Thread& operator=(const Thread&){}

    static Thread *threadins;

    class CGarbo   //它的唯一工作就是在析构函数中删除CSingleton的实例
    {
    public:
        ~CGarbo()
        {
            if(Thread::threadins)
                delete []Thread::threadins;
        }
    };
    static CGarbo Garbo;  //定义一个静态成员变量，程序结束时，系统会自动调用它的析构函数
private:
    pthread_t tid;
    long count;

public:
    static Thread* GetThreads(int n)
    {
        if(threadins == NULL)  //判断是否第一次调用
            threadins = new Thread[n];
        return threadins;
    }
public:
    pthread_t getTid(void){
        return tid;
    }
    void upperCount(void){
        ++count;
    }
};

Thread *Thread::threadins = NULL;

#endif
