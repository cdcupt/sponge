#include <stdio.h>
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

#include <signal.h>

#define SERV "0.0.0.0"
#define QUEUE 20
#define BUFF_SIZE 1024


typedef struct doc_type{
        char *key;
        char *value;
}HTTP_CONTENT_TYPE;

HTTP_CONTENT_TYPE http_content_type[] = {
        { "html","text/html" },
        { "gif" ,"image/gif" },
        { "jpeg","image/jpeg" }
};

char *http_res_tmpl = "HTTP/1.1 200 OK\r\n"
        "Server: Cleey's Server V1.0\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n\r\n";

int sockfd = -1;

void handle_signal(int sign); // 退出信号处理
void http_send(int sock_client,char *content); // http 发送相应报文
void *http_response(void *sock_client);     //http应答
void error_die(const char *sc);
int startup(u_short *port);

int main(){
        u_short port = 0;
        int sock_client = -1;
        signal(SIGINT,handle_signal);

        sockfd = startup(&port);
        printf("httpd running on port %d\n", port);

        // 客户端信息
        struct sockaddr_in claddr;
        socklen_t length = sizeof(claddr);

        while(1){
                sock_client = accept(sockfd,(struct sockaddr *)&claddr, &length);
                if( sock_client <0 ){
                        error_die("accept error");
                }

                if (pthread_create(&sock_client , NULL, &http_response, (void *)sock_client) != 0)
                    perror("pthread_create");

        }

        close(sock_client);
        exit(0);
}

void *http_response(void *sock_client){
    pthread_detach(pthread_self());

    char buff[BUFF_SIZE];
    int len;

    memset(buff, 0, sizeof(buff));
    len = recv((int)sock_client, buff, sizeof(buff),0);
    http_send((int)sock_client, "Hello World!");
}

void http_send(int sock_client,char *content){
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
         int namelen = sizeof(skaddr);
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
