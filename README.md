# 高并发httpd

---

这个项目的目的是实现一个高并发的web服务器，目前在不断的完善中，更多功能敬请期待......
## 项目简介
sponge是一个基于reactor模式的httpd，关于reactor模式，核心主要有一个事件分离器和一个事件发生器组成，他将IO事件分离开来，然后通过回调函数异步的处理IO事件。该项目为一个能处理多用户请求以及能提供较高并发量的web服务器，目前支持GET以及POST的HTTP请求，支持静态的html请求以及CGI动态页面请求以及基于FAST-CGI的php动态页面请求。
## 使用说明
编译：

    $ make
运行

    $ ./web
通过本地回环127.0.0.1以及动态分配的端口号进行HTTP请求

## 2017.3.29:
一个简单的回射服务器，使用主线程统一的accept，未使用线程池
## 2017.3.30:
新增GET命令，可以获取简单的主页信息

改为使用线程池处理并发，在线程中调用accept
## 2017.4.6:
支持POST命令，支持CGI后端框架(参考tinyserver)

CGI支持：

使用两个管道，cgi_input 和 cgi_output;

 - 在子进程中，把 STDOUT 重定向到 cgi_outputt 的写入端，把 STDIN 重定向到 cgi_input 的读取端，关闭 cgi_input 的写入端 和 cgi_output 的读取端
 - 在父进程中，关闭 cgi_input 的读取端 和 cgi_output 的写入端，如果 POST 的话，把 POST 数据写入 cgi_input，已被重定向到 STDIN，读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT。

## 2017.4.9:
在池中增加epoll复用，ET模式需使用非阻塞I/O模型，将get_line改为非阻塞I/O，异步多线程的示例运行正常。参考半同步半异步模型，计划主线程处理I/O请求，业务逻辑分离给池内线程完成，但本项目涉及业务较为简单，故采用将epoll封装在池内线程内。

## 2017.4.25:
开始重构代码，c++风格

## 2017.6.21:
重新封装Epoll类，恢复到以前的c风格代码

## 2017.6.23~2017.6.25
封装了一些用到的HTTP首部字段http_header结构体：
```c++
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
```
参考《CSAPP》一书使用健壮I/O rio封装了带缓存的read/write函数
```c++
#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                 // 内部缓冲区对应的描述符
    size_t rio_cnt;                // 可以读取的字节数
    char *rio_bufptr;           // 下一个可以读取的字节地址
    char rio_buf[RIO_BUFSIZE];  // 内部缓冲区
} rio_t;

/*从描述符fd中读取n个字节到存储器位置usrbuf*/
ssize_t rio_readn(int fd, v163oid *usrbuf, size_t n);

/*将usrbuf缓冲区中的前n个字节数据写入fd中*/
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

/*初始化内部缓冲区rio_t结构*/
void rio_readinitb(rio_t *rp, int fd);

/*系统调用read函数的包装函数,相对于read，增加了内部缓冲区*/
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);

/*从文件rp中读取一行数据（包括结尾的换行符），拷贝到usrbuf并用0字符来结束这行数据*/
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

/*从文件rp中读取n字节数据到usrbuf*/
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
```
## 2017.10.30:
修改并发模型，以前的模型将epoll封装在worker中会有惊群效应，现在维护一个链表作为线程池worker通过FIFO形式互斥的提取线程池中的任务。
```c++
if(events[i].events & EPOLLIN)
{
    int *ptr = &events[i].data.fd;
    int rc = threadpool_add(tp, accept_request, ptr);     //加入作业队列
    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
    close(events[i].data.fd);
}
```
epoll主线程将accept_request插入任务队列，由worker线程处理。

## 2017.11.15
修复一个小bug，在epoll循环中close掉epollfd导致出现errno9错误
```
~Epoll () {
    // 使用完epoll后，必须调用close() 关闭，否则可能导致fd被耗尽。
    close(epfd_);
    delete [] events_;
}
```
epollfd由析构函数自动释放

## 2018.3.2
完善log日志系统，更多状态返回与输出
```c++
class Log
{
 public:
  void log(const char *level, const char *filename, const int line, const char *format, ...);
  //返回静态实例
  static Log &get_instance(void);
};
```
相关错误状态
```c++
#define SUCCESS 0
#define MEMORY_ALLOCATION_FAILED -1
#define PTHREAD_CREATE_FILED -2
#define GETFL_ERROR -3
#define SPONGE_EAGAIN -4
#define EPOLL_ERROR -5
#define BIND_ERROR -6
#define LISTEN_ERROR -7
#define GETSOCKNAME_ERROR -8
#define WRITE_TO_CLENT_ERROR -9
#define RIO_WRITTEN_ERROR -10
#define ACCEPT_ERROR -11
#define SET_SOCKET_ERROR -12
#define INVALID_ARGUMENT -13
#define CONNECT_ERROR -14
#define SENDBEGINREQUESTRECORD_ERROR -15
#define SENDPARAMSRECORD_ERROR -16
#define SENDEMPTYPARAMSRECORD_ERROR -17
#define SENDSTDINRECORD_ERROR -18
#define SENDEMPTYSTDINRECORD_ERROR -19
#define RECVRECORD_ERROR -20
#define RIO_READN_ERROR -21
```
