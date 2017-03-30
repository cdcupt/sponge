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

#include <signal.h>

#define SERV "0.0.0.0"
#define QUEUE 20
#define BUFF_SIZE 1024
#define ISspace(x) isspace((int)(x))


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

const char *unimplemented =
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
            "Content-Type: text/html\r\n\r\n";

int sockfd = -1;

void handle_signal(int sign); // 退出信号处理
void http_send(int sock_client,const char *content); // http 发送相应报文
void *http_response(void *sock_client);     //http应答
void error_die(const char *sc);
int startup(u_short *port);
void cat(int client, FILE *resource);
int get_line(int sock, char *buf, int size);
void *accept_request(void *client);

int main(){
        u_short port = 0;
        int sock_client = -1;
        pthread_t tid;
        signal(SIGINT,handle_signal);

        sockfd = startup(&port);
        printf("server running on port %d\n", port);

        // 客户端信息
        struct sockaddr_in claddr;
        socklen_t length = sizeof(claddr);

        while(1){
                sock_client = accept(sockfd,(struct sockaddr *)&claddr, &length);
                if( sock_client <0 ){
                        error_die("accept error");
                }

                if (pthread_create(&tid , NULL, &accept_request, (void *)sock_client) != 0)
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

int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
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
        send(client,not_found,sizeof(not_found),0);
    else
    {
        //http_send(client, filename);
        send((int)client,headers,sizeof(headers),0);
        cat(client, resource);
    }
    fclose(resource);
}

void *accept_request(void *client)             //请求方法：空格：URL：协议版本：/r/n   请求行
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

        send((int)client,unimplemented,sizeof(unimplemented),0);             //change
        return 0;
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
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line((int)client, buf, sizeof(buf));
        send((int)client,not_found,sizeof(not_found),0);             //change
    }
    else
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH)    )
            cgi = 1;
        if (!cgi)
            serve_file((int)client, path);
        else;
            //execute_cgi((int)client, path, method, query_string);
    }

    close((int)client);
    return 0;
}
