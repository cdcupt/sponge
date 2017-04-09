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
#include "include/web.h"

#include <signal.h>

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;


typedef struct doc_type{
        char *key;
        char *value;
}HTTP_CONTENT_TYPE;

HTTP_CONTENT_TYPE http_content_type[] = {
        { "html","text/html" },
        { "gif" ,"image/gif" },
        { "jpeg","image/jpeg" }
};

const char *http_res_tmpl =
        "HTTP/1.1 200 OK\r\n"
        "Server: cdcupt's Server\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: Keep-Alive\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: %s\r\n\r\n";

/*const char *unimplemented =
        "HTTP/1.0 501 Method Not Implemented\r\n"
         "Server: cdcupt's Server\r\n"
         "Content-Type: text/html\r\n\r\n"
         "<HTML><HEAD><TITLE>Method Not Implemented\r\n"
         "</TITLE></HEAD>\r\n"
         "<BODY><P>HTTP request method not supported.\r\n"
         "</BODY></HTML>\r\n";

const char *not_found =
        "HTTP/1.0 404 NOT FOUND\r\n"
        "Server: cdcupt's Server\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<HTML><TITLE>Not Found</TITLE>\r\n"
        "<BODY><P>The server could not fulfill\r\n"
        "your request because the resource specified\r\n"
        "is unavailable or nonexistent.\r\n"
        "</BODY></HTML>\r\n";

const char *cannot_execute =
        "HTTP/1.0 500 Internal Server Error\r\n"
        "Content-type: text/html\r\n\r\n"
        "<P>Error prohibited CGI execution.\r\n";

const char *bad_request =
        "HTTP/1.0 400 BAD REQUEST\r\n"
        "Content-type: text/html\r\n\r\n"
        "<P>Your browser sent a bad request, "
        "such as a POST without a Content-Length.\r\n";

const char *headers =
        "HTTP/1.0 200 OK\r\n"
        "Server: cdcupt's Server\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: Keep-Alive\r\n"
        "Content-Type: text/html\r\n\r\n";*/

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
void epoll_loop(void* arg);

int sockfd = -1;
static int nthreads = 10;

//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件

struct epoll_event ev,events[1024];
int epfd;
int sock_op=1;
struct sockaddr_in address;
int n;
int i;
char buf[512];
int off;
int result;
char *p;

int main(){
        u_short port = 0;
        int i;

        sockfd = startup(&port);
        printf("server running on port %d\n", port);

        tptr = calloc(nthreads, sizeof(Thread));
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
void epoll_loop(void* arg)
{
    while(1)
    {
        n=epoll_wait(epfd,events,4096,-1);  //等待epoll事件的发生

        if(n>0)
        {
            for(i=0;i<n;++i)
            {
                if(events[i].data.fd==sockfd) //如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
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

                    tptr[(int)arg].count++;
                }
                else
                {
                    if(events[i].events&EPOLLIN)
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

int get_line(int sock, char *buf, int size)
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

    printf("Thread %d starting\n", (int) arg);
    while(1){
        pthread_mutex_lock(&mlock);
        sock_client = accept(sockfd,(struct sockaddr *)&claddr, &length);
        if( sock_client <0 ){
            error_die("accept error");
        }
        pthread_mutex_unlock(&mlock);

        tptr[(int)arg].count++;
        printf("Thread %d doing work\n", (int) arg);
        accept_request(sock_client);
        close(sock_client);
        printf("Thread %d done\n", (int) arg);
    }
}

void thread_make(int i){
    if (pthread_create(&tptr[i].tid , NULL, &epoll_loop, (void *) i) != 0)
        perror("pthread_create");
}

void *http_response(void *sock_client){
    pthread_detach(pthread_self());

    char buff[BUFF_SIZE];
    int len;

    memset(buff, 0, sizeof(buff));
    len = recv((int)sock_client, buff, sizeof(buff),0);
    http_send((int)sock_client, "Hello World!");

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
    epfd=epoll_create(65535);
    setnonblocking(sockfd); //epoll支持  I/O非阻塞
    ev.data.fd=sockfd;    //设置要处理的事件类型
    ev.events=EPOLLIN|EPOLLET;  //ev.events=EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);
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

void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
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
    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else    /* POST */
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            //send((int)client,bad_request,sizeof(bad_request),0);             //change
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        //send((int)client,cannot_execute,sizeof(cannot_execute),0);             //change
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        //send((int)client,cannot_execute,sizeof(cannot_execute),0);             //change
        cannot_execute(client);
        return;
    }

    if ( (pid = fork()) < 0 ) {
        //send((int)client,cannot_execute,sizeof(cannot_execute),0);             //change
        cannot_execute(client);
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
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);
        exit(0);
    }
    else {    /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

void accept_request(int client)             //请求方法：空格：URL：协议版本：/r/n   请求行
{
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
    char *query_string = NULL;

    numchars = get_line((int)client, buf, sizeof(buf));
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {

        //send((int)client,unimplemented,sizeof(unimplemented),0);             //change
        unimplemented(client);
        return;
    }

    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))                            //跳过空格
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))  //读取URL
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "webdocs%s", url);
    //printf("source path : %s\n", path);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {
        printf("Not found 404!\n");
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line((int)client, buf, sizeof(buf));
        //send((int)client,not_found,sizeof(not_found),0);             //change
        not_found(client);
    }
    else
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH)    )
            cgi = 1;
        if (!cgi){
            serve_file((int)client, path);
            //printf("final path : %s\n", path);
        }
        else
            execute_cgi((int)client, path, method, query_string);
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
