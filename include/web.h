#ifndef WEB_H
#define WEB_H

#include <pthread.h> //pthread线程库
#include <iostream>

#include "rio.h"
#include "fastcgi.h"

/****************************server.h*****************************/
/*
 * 请求头部结构体
 * 只存储请求行和类型、长度字段，其他信息忽略
 * 也存储了从请求地址中分析出的请求文件名和查询参数
 */
typedef struct {
    char uri[256];          // 请求地址
    char method[16];        // 请求方法
    char version[16];       // 协议版本url
    char filename[256];     // 请求文件名(包含完整路径)
    char name[256];         // 请求文件名(不包含路径，只有文件名)
    char cgiargs[256];      // 查询参数
    char contype[256];      // 请求体类型
    char conlength[16];     // 请求体长度
}hhr_t;

int send_to_cli(int fd, int outlen, char *out,
        int errlen, char *err, FCGI_EndRequestBody *endr
    );
int open_fastcgifd();
int send_fastcgi(rio_t *rp, hhr_t *hp, int sock);
int recv_fastcgi(int fd, int sock);
void execute_php(rio_t *rp, hhr_t *hp);

#define MAXLINE 8192 // 最大行长度
#define MAXBUF 8192  // io缓冲区最大值
#define LOCALBUF 1024 // 局部缓冲区大小
/*****************************************************************/

#define PORT 8888
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
