#include "csapp.h"

#define DEBUG() { printf("wwwwqqqqq\n");}
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";

typedef struct {
    char *name; //url
    int *flag;  //isused
    int *cnt;   //count
    char *object;   //content
} CacheLine;

CacheLine *cache;
int readcnt; //用来记录读者的个数
sem_t mutex, w; //mutex用来给readcnt加锁，w用来给写操作加锁

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void build_requesthdrs(rio_t *rp, char *newreq, char *hostname, char* port);
void *thread(void *vargp);
void init_cache();
int reader(int fd, char *url);
void writer(char *url, char *buf);

int main(int argc, char **argv) {
    init_cache();
    int listenfd, *connfd;   //监听描述符、建立连接描述符
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];  //主机名、端口
    struct sockaddr_storage clientaddr;
    socklen_t clientlen; 
    /* 检查命令行参数 */
    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }
    //打开一个监听套接字
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        /* Accept不断接受连接请求 */
        connfd = (int *)Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        /* 打印Accept相关信息 */
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    return 0;
}

/*handle the client HTTP transaction*/
void doit(int connfd) {
    int endserver_fd;   /* 被代理服务器文件描述符 */
    char object_buf[MAX_OBJECT_SIZE];
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], uri[MAXLINE], version[MAXLINE];
    /*store the request line arguments*/
    char hostname[MAXLINE], path[MAXLINE];
    int port;
    rio_t from_client, to_endserver;
    /* 建立rio关联，再读取请求行 */
    Rio_readinitb(&from_client, connfd);
    Rio_readlineb(&from_client, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    strcpy(url, uri);
    if (strcasecmp(method, "GET")) {                     
        clienterror(connfd, method, "501", "Not Implemented",
                    "Proxy Server does not implement this method");
        return;
    }
    if (reader(connfd, url)) {
        fprintf(stdout, "%s from cache\n", url);
        return;
    }
    /*从uri中解析得到 hostname path和port(port默认是80)*/
    parse_uri(uri, hostname, path, &port);

    char port_str[10];
    sprintf(port_str, "%d", port);
    endserver_fd = Open_clientfd(hostname, port_str);
    Rio_readinitb(&to_endserver, endserver_fd);
    //newreq是新的请求头
    char newreq[MAXLINE];
    sprintf(newreq, "GET %s HTTP/1.0\r\n", path); 
    build_requesthdrs(&from_client, newreq, hostname, port_str);
    //发送请求行给被代理服务器

    // printf("%s %s\n", hostname, port_str);
    // printf("%s", newreq);
    // printf("-------------------------------------------");

    Rio_writen(endserver_fd, newreq, strlen(newreq));
    int n;
    //从被代理服务器读取的信息发送给客户端
    int total_size = 0;
    while ((n = Rio_readlineb(&to_endserver, buf, MAXLINE))) {
        Rio_writen(connfd, buf, n);
        if (total_size + n < MAX_OBJECT_SIZE)
            strcpy(object_buf + total_size, buf);
        total_size += n;
    }
    if (total_size < MAX_OBJECT_SIZE)
        writer(url, object_buf);
    
    Close(endserver_fd);
}

/*parse the uri to get hostname,file path ,port*/
/*无法处理这种带get参数的奇葩的uri
    http://znsv.baidu.com/customer_search/api/ping?logid=3148635800&version=1.0&prod_id=cse&plate_url=http://home.baidu.com/home
    /index/contact_us&referrer=&time=1604631268402&page_id=content_page&source=new&site_id=6706059176758103565
*/
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    *port = 80;
    //uri   http://www.cmu.edu:8080/hub/index.html
    //pos1  www.cmu.edu'\0'
    //pos2  /hub/index.html'\0'

    char* pos1 = strstr(uri,"//");
    if (pos1 == NULL) 
        pos1 = uri;
    else 
        pos1 += 2;
    
    char* pos2 = strstr(pos1, ":");
    if (pos2 != NULL) {
        *pos2 = '\0';
        strncpy(hostname, pos1, MAXLINE);
        sscanf(pos2 + 1,"%d%s", port, path);
    } else {
        pos2 = strstr(pos1,"/");
        if (pos2 == NULL) {
            strncpy(hostname, pos1, MAXLINE);
            strcpy(path,"");
            return;
        }
        *pos2 = '\0';
        strncpy(hostname, pos1, MAXLINE);
        *pos2 = '/';
        strncpy(path, pos2, MAXLINE);
    }

    return;
}


void clienterror(int fd, char *cause, char *errnum, 
         char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void build_requesthdrs(rio_t *rp, char *newreq, char *hostname, char* port) {
    //already have sprintf(newreq, "GET %s HTTP/1.0\r\n", path);
    char buf[MAXLINE];

    while(Rio_readlineb(rp, buf, MAXLINE) > 0) {          
        if (!strcmp(buf, "\r\n")) break;
        if (strstr(buf,"Host:") != NULL) continue;
        if (strstr(buf,"User-Agent:") != NULL) continue;
        if (strstr(buf,"Connection:") != NULL) continue;
        if (strstr(buf,"Proxy-Connection:") != NULL) continue;
        sprintf(newreq,"%s%s", newreq, buf);
    }
    sprintf(newreq, "%sHost: %s:%s\r\n", newreq, hostname, port);
    sprintf(newreq, "%s%s%s%s", newreq, user_agent_hdr, conn_hdr, prox_hdr);
    sprintf(newreq,"%s\r\n", newreq);
}

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    /* 执行事务 */
    doit(connfd);
    /* 关闭连接 */
    Close(connfd);
    return NULL;
}

void init_cache() {
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    readcnt = 0;
    cache = (CacheLine *)Malloc(sizeof(CacheLine) * 10);
    for (int i = 0; i < 10; i++) {
        cache[i].name = (char *)Malloc(sizeof(char) * 256);
        cache[i].flag = (int *)Malloc(sizeof(int));
        cache[i].cnt = (int *)Malloc(sizeof(int));
        cache[i].object = (char *)Malloc(sizeof(char) * MAX_OBJECT_SIZE);
        *(cache[i].flag) = 0;
        *(cache[i].cnt) = 0;
    }
}

int reader(int fd, char *url) {
    int in_cache = 0;
    P(&mutex);
    readcnt++;
    if (readcnt == 1) P(&w);
    V(&mutex);
    
    for (int i = 0; i < 10; ++i) {
        if (*(cache[i].flag) == 1 && !strcmp(cache[i].name, url)) { //命中
            Rio_writen(fd, cache[i].object, MAX_OBJECT_SIZE);
            in_cache = 1;
            *(cache[i].cnt) = 0;
            break;
        }
    }
    for (int i = 0; i < 10; i++)
        (*(cache[i].cnt))++;
    P(&mutex);
    readcnt--;
    if (readcnt == 0)
        V(&w);
    V(&mutex);
    return in_cache;
}

void writer(char *url, char *buf) {
    int in_cache = 0;
    P(&w);
    for (int i = 0; i < 10; ++i) {
        if (*(cache[i].flag) == 1 && !strcmp(cache[i].name, url)) { //命中
            in_cache = 1;
            *(cache[i].cnt) = 0;
            break;
        }
    }
    //未命中替换或者插入
    if (in_cache == 0) {
        int ind = 0;
        int max_cnt = 0;
        for (int i = 0; i < 10; ++i) {
            if (*(cache[i].flag) == 0) {    //unused
                ind = i;
                break;
            }
            if (*(cache[i].cnt) > max_cnt) {
                ind = i;
                max_cnt = *(cache[i].cnt);
            }
        }
        *(cache[ind].flag) = 1;
        strcpy(cache[ind].name, url);
        strcpy(cache[ind].object, buf);
        *(cache[ind].cnt) = 0;
    }
    for (int i = 0; i < 10; i++)
        (*(cache[i].cnt))++;
    V(&w);
}


/* test
    telnet 127.0.0.1 4500
    GET http://www.cmu.edu/hub/index.html HTTP/1.1
    curl -v --proxy http://localhost:4501 http://localhost:4502/home.html
    GET http://www.cmu.edu:80/hub/index.html HTTP/1.1
    GET /hub/index.html HTTP/1.0
    
*/