#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h> //epoll支持

class Epoll {
public:
    Epoll (uint32_t m_ = 256) {
      events_ = new struct epoll_event [m_];
      memset(events_, 0, sizeof(struct epoll_event));

      // int epoll_create(int size);
      // 创建一个epoll的句柄，size用来告诉内核需要监听的数目一共有多大。当创建好epoll句柄后，
      // 它就是会占用一个fd值，在linux下如果查看/proc/进程id/fd/，是能够看到这个fd的，
      // 所以在使用完epoll后，必须调用close() 关闭，否则可能导致fd被耗尽。
      epfd_ = epoll_create(m_);
      if (-1 == epfd_) {
        fprintf(stderr,"epoll create failed");
      }
    }
    ~Epoll () {
      // 使用完epoll后，必须调用close() 关闭，否则可能导致fd被耗尽。
      close(epfd_);
      delete [] events_;
    }
    /*
     *  int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
     *  epoll的事件注册函数，第一个参数是 epoll_create() 的返回值，
     *  第二个参数表示动作:
     *    EPOLL_CTL_ADD    注册新的fd到epfd中；
     *    EPOLL_CTL_MOD    修改已经注册的fd的监听事件；
     *    EPOLL_CTL_DEL    从epfd中删除一个fd；
     *  第三个参数是需要监听的fd，
     *  第四个参数是告诉内核需要监听什么事:
     *    EPOLLIN      表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
     *    EPOLLOUT     表示对应的文件描述符可以写；
     *    EPOLLPRI     表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
     *    EPOLLERR     表示对应的文件描述符发生错误；
     *    EPOLLHUP     表示对应的文件描述符被挂断；
     *    EPOLLET      将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
     *    EPOLLONESHOT 只监听一次事件，当监听完这次事件之后,
     *                 如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里。
     */
    void Ctrl (int op, int fd, uint32_t event) {
      ev_.data.fd = fd;
      ev_.events = event;
      epoll_ctl(epfd_, op, fd, &ev_);
    }
    /*
     *  int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
     *  参数events用来从内核得到事件的集合，maxevents 告之内核这个events有多大，
     *  这个 maxevents 的值不能大于创建 epoll_create() 时的size，
     *  参数 timeout 是超时时间（毫秒，0会立即返回，-1将不确定，也有说法说是永久阻塞）,
     *  该函数返回需要处理的事件数目，如返回0表示已超时。
     */
    int Poll (int maxevents, int timeout) {
      return epoll_wait(epfd_, events_, maxevents, timeout);
    }
    struct epoll_event* GetEvents() {
      return events_;
    }

    struct epoll_event GetEv(){
        return ev_;
    }

    int GetEpfd(){
        return epfd_;
    }
    /*
     * typedef union epoll_data
     * {
     *   void        *ptr;
     *   int          fd;
     *   __uint32_t   u32;
     *   __uint64_t   u64;
     * } epoll_data_t;
     *
     * struct epoll_event {
     * __uint32_t events; // Epoll events
     * epoll_data_t data; //User data variable
     * };
     */
private:
    int epfd_;
    struct epoll_event *events_;
    struct epoll_event ev_;
};

#endif
