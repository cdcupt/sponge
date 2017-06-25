#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h> // socket
#include <sys/types.h>  // 基本数据类型
#include <unistd.h> // read write
#include <string.h>
#include <stdlib.h>
#include <fcntl.h> // open close
#include <sys/shm.h>
#include <arpa/inet.h>
#include <pthread.h> //pthread线程库
#include <ctype.h> //isspace函数
#include <sys/stat.h> //stat函数
#include <sys/wait.h> //waitpid函数
#include <sys/epoll.h> //epoll支持
#include <errno.h>
#include "web.h"
#include "epoll.h"

#include "rio.h"
#include "fastcgi.h"

#include <signal.h>

#include <iostream>

using namespace std;

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;


typedef struct doc_type{
        char *key;
        char *value;
}HTTP_CONTENT_TYPE;

const char *http_res_tmpl =
        "HTTP/1.1 200 OK\r\n"
        "Server: cdcupt's Server\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: Keep-Alive\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: %s\r\n\r\n";

void headers(int client, const char *filename);
void cat(int client, FILE *resource);
void bad_request(int client);
void cannot_execute(int client);
void not_found(int client);
void unimplemented(int client);
void handle_signal(int sign); // 退出信号处理
void http_send(int sock_client,const char *content); // http 发送相应报文
void *http_response(void *sock_client);     //http应答
void error_die(const char *sc);
int startup(u_short *port);
int get_line(int sock, char *buf, int size);
//int get_line_noblock(int sock, char *buf, int size); //非阻塞版本
void serve_file(int client, const char *filename);
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string);
void accept_request(int client);
void thread_main(void *arg);
void thread_make(int i);

//epoll
void setnonblocking(int sock);
void *epoll_loop(void* arg);

int sockfd = -1;
static int nthreads = 10;

//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件

/*struct epoll_event ev,events[1024];
int epfd;*/
Epoll epoll_(1024);                  //epoll类

Thread *tptr = Thread::GetThreads(nthreads);

int main(){
        u_short port = 0;
        int i;

        sockfd = startup(&port);
        printf("server running on port %d\n", port);

        for(i=0; i<nthreads; ++i){
            thread_make(i);
        }

        signal(SIGINT,handle_signal);

        while(1){
            pause();
        }

        exit(0);
}

void setnonblocking(int sock)
{
    int opts;
    opts=fcntl(sock,F_GETFL);
    if(opts<0)
    {
         perror("fcntl(sock,GETFL)");
         exit(1);
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock,F_SETFL,opts)<0)
    {
         perror("fcntl(sock,SETFL,opts)");
         exit(1);
    }
}

//epoll检测循环
void *epoll_loop(void* arg)
{
    struct epoll_event *events = epoll_.GetEvents();
    struct epoll_event ev = epoll_.GetEv();
    int epfd = epoll_.GetEpfd();
    while(1)
    {
        int n=epoll_.Poll(4096,-1);  //等待epoll事件的发生

        if(n>0)
        {
            for(int i=0;i<n;++i)
            {
                if(events[i].data.fd == sockfd) //如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
                {

                    pthread_mutex_lock(&mlock);
                    ev.data.fd=accept(sockfd,NULL,NULL);
                    if(ev.data.fd>0)
                    {
                        setnonblocking(ev.data.fd);
                        ev.events=EPOLLIN|EPOLLET;
                        epoll_ctl(epfd,EPOLL_CTL_ADD,ev.data.fd,&ev);
                    }
                    else
                    {
                        if(errno==EAGAIN)
                            break;
                    }
                    pthread_mutex_unlock(&mlock);

                    tptr[(long)arg].upperCount();
                }
                else
                {
                    if(events[i].events & EPOLLIN)
                    {
                        accept_request(events[i].data.fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                        close(events[i].data.fd);
                    }
                    else if(events[i].events&EPOLLOUT)
                    {
                        serve_file(events[i].data.fd, "webdocs/index.html");
                        close(events[i].data.fd);
                    }
                    else
                    {
                        close(events[i].data.fd);
                    }
                }
            }
        }
    }
}

/*int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
    return(i);
}*/

int get_line(int sock, char *buf, int size)
{
    struct epoll_event *events = epoll_.GetEvents();
    struct epoll_event ev = epoll_.GetEv();
    int epfd = epoll_.GetEpfd();

    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, 0);
                if(n<=0 || c!='\n'){
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else{
            if(errno == EAGAIN)
            {
                c = '\n';
                printf("EAGAIN\n");
                break;
            }
            else{
                printf("recv error!\n");
                epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                close(events[i].data.fd);
                break;
            }
        }
    }
    buf[i] = '\0';

    return(i);
}

void thread_main(void *arg){
    int sock_client = -1;

    // 客户端信息
    struct sockaddr_in claddr;
    socklen_t length = sizeof(claddr);

    printf("Thread %ld starting\n", (long) arg);
    while(1){
        pthread_mutex_lock(&mlock);
        sock_client = accept(sockfd,(struct sockaddr *)&claddr, &length);
        if( sock_client <0 ){
            error_die("accept error");
        }
        pthread_mutex_unlock(&mlock);

        tptr[(long)arg].upperCount();
        printf("Thread %ld doing work\n", (long) arg);
        accept_request(sock_client);
        close(sock_client);
        printf("Thread %ld done\n", (long) arg);
    }
}

void thread_make(int i){
    pthread_t tid = tptr[i].getTid();
    if (pthread_create(&tid , NULL, epoll_loop, (void *) i) != 0)
        perror("pthread_create");
}

void *http_response(void *sock_client){
    pthread_detach(pthread_self());

    char buff[BUFF_SIZE];

    memset(buff, 0, sizeof(buff));
    int len = recv((long)sock_client, buff, sizeof(buff),0);
    http_send((long)sock_client, "Hello World!");

    return 0;
}

void http_send(int sock_client, const char *content){
    char HTTP_HEADER[BUFF_SIZE],HTTP_INFO[BUFF_SIZE];
    int len = strlen(content);
    sprintf(HTTP_HEADER,http_res_tmpl,len,"text/html");
    len = sprintf(HTTP_INFO,"%s%s",HTTP_HEADER,content);

    send(sock_client,HTTP_INFO,len,0);
}

void handle_signal(int sign){
    fputs("\nSIGNAL INTERRUPT \nBye Cleey! \nSAFE EXIT\n",stdout);
    close(sockfd);
        exit(0);
}

int startup(u_short *port)
{
    int sockfd = 0;
    // 定义 socket
    sockfd = socket(AF_INET,SOCK_STREAM,0);

    /****************************************************///epoll初始化
    /*epfd=epoll_create(65535);
    setnonblocking(sockfd); //epoll支持  I/O非阻塞
    ev.data.fd=sockfd;    //设置要处理的事件类型
    ev.events=EPOLLIN|EPOLLET;  //ev.events=EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);*/
    /****************************************************/

    /****************************************************///epoll初始化,类实例
    setnonblocking(sockfd); //epoll支持  I/O非阻塞
    epoll_.Ctrl(EPOLL_CTL_ADD, sockfd, EPOLLIN|EPOLLET);
    /****************************************************/

    // 定义 sockaddr_in
    struct sockaddr_in skaddr;
    skaddr.sin_family = AF_INET; // ipv4
    skaddr.sin_port   = htons(*port);
    skaddr.sin_addr.s_addr = inet_addr(SERV);
    // bind，绑定 socket 和 sockaddr_in
    if( bind(sockfd,(struct sockaddr *)&skaddr,sizeof(skaddr)) == -1 ){
            error_die("bind error");
    }
     if (*port == 0)  /* if dynamically allocating a port */
     {
         socklen_t namelen = sizeof(skaddr);
         if (getsockname(sockfd, (struct sockaddr *)&skaddr, &namelen) == -1)
            error_die("getsockname");
         *port = ntohs(skaddr.sin_port);
     }
     // listen，开始添加端口
     if( listen(sockfd,QUEUE) == -1 ){
             error_die("listen error");
     }
     return(sockfd);
}

void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        //send(client,not_found,sizeof(not_found),0);
        not_found(client);
    else
    {
        //http_send(client, filename);
        //send(client,headers,sizeof(headers),0);
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

void execute_cgi(rio_t *rp, hhr_t *hp, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(hp->method, "POST") == 0)    /* POST */
    {
        char* len = hp->conlength;
        content_length = atoi(len);
        if (content_length == -1) {
            //send((int)client,bad_request,sizeof(bad_request),0);             //change
            bad_request(rp->rio_fd);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(rp->rio_fd, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        //send((int)client,cannot_execute,sizeof(cannot_execute),0);             //change
        cannot_execute(rp->rio_fd);
        return;
    }
    if (pipe(cgi_input) < 0) {
        //send((int)client,cannot_execute,sizeof(cannot_execute),0);             //change
        cannot_execute(rp->rio_fd);
        return;
    }

    if ( (pid = fork()) < 0 ) {
        //send((int)client,cannot_execute,sizeof(cannot_execute),0);             //change
        cannot_execute(rp->rio_fd);
        return;
    }
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);
        close(cgi_output[0]);
        close(cgi_input[1]);
        sprintf(meth_env, "REQUEST_METHOD=%s", hp->method);
        putenv(meth_env);
        if (strcasecmp(hp->method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(hp->name, hp->name, NULL);
        exit(0);
    }
    else {    /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(hp->method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(rp->rio_fd, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(rp->rio_fd, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

/*****************************fastcgi-php-fpm**********************************/
/*
 * php处理结果发送给客户端
 */
int send_to_cli(int fd, int outlen, char *out,
        int errlen, char *err, FCGI_EndRequestBody *endr
        )
{
    char *p;
    int n;

    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Zhou Web Server\r\n", buf);
    sprintf(buf, "%sContent-Length: %d\r\n", buf, outlen + errlen);
    sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, "text/html");
    if (rio_writen(fd, buf, strlen(buf)) < 0) {
        cout << "write to client error" << endl;
    }

    if (outlen > 0) {
        p = index(out, '\r');
        n = (int)(p - out);
        if (rio_writen(fd, p + 3, outlen - n - 3) < 0) {
            cout << "rio_written error" << endl;
            return -1;
        }
    }

    if (errlen > 0) {
        if (rio_writen(fd, err, errlen) < 0) {
            cout << "rio_written error" << endl;
            return -1;
        }
    }

    return 0;
}

int open_fastcgifd() {
    int sock;
	struct sockaddr_in serv_addr;

    // 创建套接字
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (-1 == sock) {
        cout << "socket error" << endl;
        return -1;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(FCGI_HOST);
	serv_addr.sin_port = htons(FCGI_PORT);

    // 连接服务器
	if(-1 == connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))){
        cout << "connect error" << endl;
        return -1;
	}

    return sock;
}

/*
 * 发送http请求行和请求体数据给fastcgi服务器
 */
int send_fastcgi(rio_t *rp, hhr_t *hp, int sock)
{
    int requestId, i, l;
    char *buf;

    requestId = sock;

    // params参数名
    char *paname[] = {
        "SCRIPT_FILENAME",
        "SCRIPT_NAME",
        "REQUEST_METHOD",
        "REQUEST_URI",
        "QUERY_STRING",
        "CONTENT_TYPE",
        "CONTENT_LENGTH"
    };

    // 对应上面params参数名，具体参数值所在hhr_t结构体中的偏移
    int paoffset[] = {
        (size_t) & (((hhr_t *)0)->filename),
        (size_t) & (((hhr_t *)0)->name),
        (size_t) & (((hhr_t *)0)->method),
        (size_t) & (((hhr_t *)0)->uri),
        (size_t) & (((hhr_t *)0)->cgiargs),
        (size_t) & (((hhr_t *)0)->contype),
        (size_t) & (((hhr_t *)0)->conlength)
    };

    // 发送开始请求记录
    if (sendBeginRequestRecord(rio_writen, sock, requestId) < 0) {
        cout << "sendBeginRequestRecord error" << endl;
        return -1;
    }

    // 发送params参数
    l = sizeof(paoffset) / sizeof(paoffset[0]);
    for (i = 0; i < l; i++) {
        // params参数的值不为空才发送
        if (strlen((char *)(((long)hp) + paoffset[i])) > 0) {
            if (sendParamsRecord(rio_writen, sock, requestId, paname[i], strlen(paname[i]),
                        (char *)(((long)hp) + paoffset[i]),
                        strlen((char *)(((long)hp) + paoffset[i]))) < 0) {
                cout << "sendParamsRecord error" << endl;;
                return -1;
            }
        }
    }

    // 发送空的params参数
    if (sendEmptyParamsRecord(rio_writen, sock, requestId) < 0) {
        cout << "sendEmptyParamsRecord error" << endl;
        return -1;
    }

    // 继续读取请求体数据
    l = atoi(hp->conlength);
    if (l > 0) { // 请求体大小大于0
        buf = (char *)malloc(l + 1);
        memset(buf, '\0', l);
        if (rio_readnb(rp, buf, l) < 0) {
            cout << "rio_readn error" << endl;
            free(buf);
            return -1;
        }

        // 发送stdin数据
        if (sendStdinRecord(rio_writen, sock, requestId, buf, l) < 0) {
            cout << "sendStdinRecord error" << endl;
            free(buf);
            return -1;
        }

        free(buf);
    }

    // 发送空的stdin数据
    if (sendEmptyStdinRecord(rio_writen, sock, requestId) < 0) {
        cout << "sendEmptyStdinRecord error" << endl;
        return -1;
    }

    return 0;
}

/*
 * 接收fastcgi返回的数据
 */
int recv_fastcgi(int fd, int sock) {
    int requestId;
    char *p;
    int n;

    requestId = sock;

    // 读取处理结果
    if (recvRecord(rio_readn, send_to_cli, fd, sock, requestId) < 0) {
        cout << "recvRecord error" << endl;

        return -1;
    }

    return 0;
}

void execute_php(rio_t *rp, hhr_t *hp) {
    int sock;

    // 创建一个连接到fastcgi服务器的套接字
    sock = open_fastcgifd();

    // 发送http请求数据
    send_fastcgi(rp, hp, sock);

    // 接收处理结果
    recv_fastcgi(rp->rio_fd, sock);

    close(sock); // 关闭与fastcgi服务器连接的套接字
}

/*
 * 判断str起始位置开始是否包含"content-type"
 * 包含返回1
 * 不包含返回0
 */
static int is_contype(char *str)
{
    char *cur = str;
    char *cmp = "content-type";

    // 删除开始的空格
    while (*cur == ' ') {
        cur++;
    }

    for (; *cmp != '\0' && tolower(*cur) == *cmp; cur++,cmp++);

    if (*cmp == '\0') { // cmp字符串以0结束
        return 1;
    }

    return 0;
}

/*
 * 判断str起始位置开始是否包含"content-length"
 * 包含返回1
 * 不包含返回0
 */
static int is_conlength(char *str)
{
    char *cur = str;
    char *cmp = "content-length";

    // 删除开始的空格
    while (*cur == ' ') {
        cur++;
    }

    for (; *cmp != '\0' && tolower(*cur) == *cmp; cur++,cmp++);

    if (*cmp == '\0') { // cmp字符串以0结束
        return 1;
    }

    return 0;
}
/******************************************************************************/

void accept_request(int client)             //请求方法：空格：URL：协议版本：/r/n   请求行
{
    char buf[1024];
    int numchars;
    //char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int dynamic = 0; //state 1 :cgi程序； state 2 ：php程序
    char query_string[512];

    /*************************************************************************/
    char *query;
    char *php = ".php"; // 根据后缀名判断是静态页面还是动态页面
    char *cgi = ".cgi"; // 根据后缀名判断是静态页面还是动态页面

    char cwd[1024];

    hhr_t hhr;
    rio_t rio;

    memset(&hhr, 0, sizeof(hhr));
    memset(&rio, 0, sizeof(rio));

    rio_readinitb(&rio, client);
    /*************************************************************************/

    memset(buf, 0, 1024);
    get_line((int)client, buf, sizeof(buf));

    // 提取请求方法、请求URI、HTTP版本
    sscanf(buf, "%s %s %s", hhr.method, hhr.uri, hhr.version);
    char urin[1024];
    strcpy(urin, hhr.uri); // 不破坏原始字符串

    if (strcasecmp(hhr.method, "GET") && strcasecmp(hhr.method, "POST"))
    {
        //send((int)client,unimplemented,sizeof(unimplemented),0);             //change
        unimplemented(client);
        return;
    }

    //获取头部信息
/******************************************************************************/
    memset(buf, 0, 1024);
    get_line((int)client, buf, sizeof(buf));

    char *start, *end;
    while (errno != EAGAIN) {
        start = index(buf, ':');
        // 每行数据包含\r\n字符，需要删除
        end = index(buf, '\r');
        if (start != 0 && end != 0) {
            *end = '\0';
            while ((*(start + 1)) == ' ') {
                start++;
            }

            if (is_contype(buf)) {
                strcpy(hhr.contype, start + 1);
            } else if (is_conlength(buf)) {
                strcpy(hhr.conlength, start + 1);
            }
        }
        memset(buf, 0, 1024);
        get_line((int)client, buf, sizeof(buf));
    }
/******************************************************************************/
//cout << "query_string: " << query_string << endl;
    if (strcasecmp(hhr.method, "GET") == 0)
    {
        cout << "urin: " << urin << endl;
        for(int i=0; urin[i]!='\0'; ++i){
            if(urin[i] == '?'){
                urin[i] = '\0';                             //将路径从字符'?'截断
                int j;
                for(j=i+1; urin[j]!='\0'; ++j){
                    query_string[j-i-1] = urin[j];
                }
                query_string[j] = '\0';
            }
        }
    }

    sprintf(path, "webdocs%s", urin);
    //printf("source path : %s\n", path);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {
        printf("Not found 404!\n");
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line((int)client, buf, sizeof(buf));
        if(errno == EAGAIN) cout << "808 error!" << endl;
        //send((int)client,not_found,sizeof(not_found),0);             //change
        not_found(client);
    }
    else
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH) )
        {
                if ((query = strstr(urin, php))) dynamic = 2;
                else  dynamic = 1;
        }
        else {
            strcpy(hhr.cgiargs, "");
        }

/******************************************************************************/
        char* dir = getcwd(cwd, 1024); // 获取当前工作目录
        strcpy(hhr.filename, dir);       // 包含完整路径名
        strcat(hhr.filename, path);
        strcpy(hhr.name, path);         // 不包含完整路径名
/******************************************************************************/

        if (dynamic == 0){
            serve_file((int)client, path);
            //printf("final path : %s\n", path);
        }
        else if(dynamic == 1){
            cout << "path: " << hhr.name << endl;
            cout << "method: " << hhr.method << endl;
            cout << "conlength: " << hhr.conlength << endl;
            cout << "query_string: " << query_string << endl;
            execute_cgi(&rio, &hhr, query_string);
        }
        else{
            execute_php(&rio, &hhr);
        }
    }

    close((int)client);
}

void bad_request(int client)
{
     char buf[1024];

     sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
     send(client, buf, sizeof(buf), 0);
     sprintf(buf, "Content-type: text/html\r\n");
     send(client, buf, sizeof(buf), 0);
     sprintf(buf, "\r\n");
     send(client, buf, sizeof(buf), 0);
     sprintf(buf, "<P>Your browser sent a bad request, ");
     send(client, buf, sizeof(buf), 0);
     sprintf(buf, "such as a POST without a Content-Length.\r\n");
     send(client, buf, sizeof(buf), 0);
}

void cannot_execute(int client)
{
     char buf[1024];

     sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "Content-type: text/html\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
     send(client, buf, strlen(buf), 0);
}

void headers(int client, const char *filename)
{
     char buf[1024];
     (void)filename;  /* could use filename to determine file type */

     strcpy(buf, "HTTP/1.0 200 OK\r\n");
     send(client, buf, strlen(buf), 0);
     strcpy(buf, "Server: cdcupt's Server\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "Content-Type: text/html\r\n");
     send(client, buf, strlen(buf), 0);
     strcpy(buf, "\r\n");
     send(client, buf, strlen(buf), 0);
}

void not_found(int client)
{
     char buf[1024];
     FILE *resource = NULL;

     sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "Server: cdcupt's Server\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "Content-Type: text/html\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "\r\n");
     send(client, buf, strlen(buf), 0);

     resource = fopen("webdocs/404notfound.html", "r");
     cat(client, resource);
     fclose(resource);
}

void unimplemented(int client)
{
     char buf[1024];

     sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "Server: cdcupt's Server\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "Content-Type: text/html\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "</TITLE></HEAD>\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
     send(client, buf, strlen(buf), 0);
     sprintf(buf, "</BODY></HTML>\r\n");
     send(client, buf, strlen(buf), 0);
}
