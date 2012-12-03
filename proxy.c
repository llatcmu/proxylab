#include <stdio.h>
#include "string.h"
#include "csapp.h"
#include "pcache.h"

#define MAX_CACHE_SIZE  110000
#define MAX_OBJECT_SIZE 102400

#define HOST             0
#define USER_AGENT       1
#define ACCEPT           2
#define ACCEPT_ENCODING  3
#define CONNECTION       4
#define PROXY_CONNECTION 5

#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept1 = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";
static const char *get = "GET ";
static const char *version = " HTTP/1.0\r\n";

sem_t mutex;
sem_t cache_mutex;
pthread_rwlock_t rwlock;


void *thread(void *vargp);
unsigned int getHostname(char *uri);
void *getIndex(char *p, char *index);
void doit(int fd);
int open_clientfd_with_mutex(char *hostname, int port);
int *generate_request_header(char *requestHeader, char *buf1, int *flags);
void *ajust_request_header(int *flags, char *hostname, char *requestHeader);
void *generate_request(char *request, char *uri, char *method, char *hostname,
                        int *serverport); 

int main(int argc, char **argv) 
{   

    int listenfd, port, *connfdp;
    pthread_t tid;
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);

    if (argc != 2) {
        printf("Argument is invalid.\n");
        exit(1);
    }
    port = atoi(argv[1]);

    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(port);
    sem_init(&mutex, 0, 1);
    sem_init(&cache_mutex, 0, 1);
    pthread_rwlock_init(&rwlock, NULL);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    return 0;
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int fd) 
{
    rio_t rio1, rio2;
    char buf1[MAXLINE] = {0}, buf2[MAX_OBJECT_SIZE] = {0}; 
    char method[MAXLINE] = {0}, uri[MAXLINE] = {0};
    char hostname[MAXLINE] = {0}, request[MAX_OBJECT_SIZE] = {0};
    char requestHeader[MAX_OBJECT_SIZE];
    char *cacheBuffer;   
    linePCache *cacheLine;
    
    int datasize = 0;
    int exceeded_size = 0;
    int n, clientfd, serverport = 80;
    int  flags[6] = {0, 0, 0, 0, 0, 0};



    rio_readinitb(&rio1, fd);    
    if (rio_readlineb(&rio1, buf1, MAXLINE) <= 0)
        return;

    sscanf(buf1, "%s %s", method, uri);
    if (strcmp(method, "GET")) {
        printf("We only can deal with GET method.\n");
        return;
    }

    generate_request(request, uri, method, hostname, &serverport);

    memset(buf1, 0, sizeof(buf1));
    if ((n = rio_readlineb(&rio1, buf1, MAXLINE)) < 0 ) {
        printf("read from client error\n");
        return ;
    }   
    while(strcmp(buf1, "\r\n")) {
        generate_request_header(requestHeader, buf1, flags);
        memset(buf1, 0, sizeof(buf1));
        if ((n = rio_readlineb(&rio1, buf1, MAXLINE)) < 0 ) {
            printf("read from client error\n");
            return;
        }       
    }
    
    ajust_request_header(flags, hostname, requestHeader); 

    strcat(request, requestHeader);
    strcat(request, "\r\n");

    dbg_printf("go into lock\n");

    //pthread_rwlock_wrlock(&rwlock);
    P(&cache_mutex);

    dbg_printf("start get/write\n");
    cacheLine = get_webobj_from(uri);
    dbg_printf("end get/write\n");
    //pthread_rwlock_unlock(&rwlock);

    V(&cache_mutex);
    dbg_printf("out of lock\n");

    if (cacheLine != NULL) {
        cacheBuffer = cacheLine->webobj_buf;
        dbg_printf("cache not NULL, size: %d\n", cacheLine->obj_length);
        dbg_printf("Sizeof return value: %lu\n", sizeof(cacheBuffer));
        if ((n = rio_writen(fd, cacheBuffer, cacheLine->obj_length)) < 0) {
            printf("rio_writen error, can not send data to"  
                "client from cache\n");
            return;
        }    
    }

    else {
        if ((clientfd = open_clientfd_with_mutex(hostname, serverport)) < 0) {
            printf("request uri error, cannot connect to the server\n");
            return;
        }

        if ((n = rio_writen(clientfd, request, strlen(request))) < 0) {
            printf("Forward request to server error\n");
            Close(clientfd);
            return;
        }

        rio_readinitb(&rio2, clientfd);
        dbg_printf("Sizeof return value of buff2: %lu\n", sizeof(buf2));
        memset(buf2, 0, sizeof(buf2));
        datasize = 0;
        exceeded_size = 0;

        while ((n = rio_readnb(&rio2, buf2, MAX_OBJECT_SIZE)) > 0) {
            if (exceeded_size)
            {
                memset(buf2, 0, sizeof(buf2));   
            }
            rio_writen(fd, buf2, n);
            datasize += n;  
            exceeded_size ++;          
        }
        if (exceeded_size > 1 && datasize < MAX_OBJECT_SIZE)
        {
            dbg_printf("datasize: %d, exceeded_size: %d\n", datasize, exceeded_size);
            exit(0);
        }
        dbg_printf("datasize: %d\n", datasize);

        if (datasize < MAX_OBJECT_SIZE) {
            //dbg_printf("before memcpy, from %p, to %p", buf2, cachePoint);
            //memcpy(cachePoint, buf2, datasize);
            P(&cache_mutex);
            set_webobj_to(uri, buf2, datasize);
            V(&cache_mutex);
        }

        if (n < 0) {
            Close(clientfd);
            return;
        }        
        Close(clientfd);
    }    
}

int open_clientfd_with_mutex(char *hostname, int port) 
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    P(&mutex);
    if ((hp = gethostbyname(hostname)) == NULL) {
        V(&mutex);
        return -2; /* check h_errno for cause of error */
    }
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0], 
      (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);
    V(&mutex);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
    return -1;
    return clientfd;
}

void *generate_request(char *request, char *uri, char *method, char *hostname,
                        int *serverport) 
{
    char uriForward[MAXLINE];
    char *p; 

    strncpy(hostname, uri, getHostname(uri));
    p = memchr(hostname, '/', strlen(hostname));
    p = memchr(p + 1, '/', strlen(hostname));
    strcpy(hostname, p + 1);
    if ((p = memchr(hostname, ':', strlen(hostname))) != NULL) {
        *serverport = atoi(p + 1);
        strtok(hostname, ":");
    }

    strcpy(uriForward, uri + getHostname(uri));

    printf("hostname = %s, serverport = %d, uriForward = %s\n",
             hostname, *serverport, uriForward);

    memset(request, 0, sizeof(request));
    strcat(request, get);
    strcat(request, uriForward);
    strcat(request, version);

    return request;
}

int *generate_request_header(char *requestHeader, char *buf1, int *flags)
{   
    char index[MAXLINE];

    getIndex(buf1, index);
    if (!strcmp("Host", index)) {
        flags[HOST] = 1;
        strcat(requestHeader, buf1);
    }
    else if (!strcmp("User-Agent", index)) {
        flags[USER_AGENT] = 1;
        strcat(requestHeader, user_agent);
    }
    else if (!strcmp("Accept", index)) {
        flags[ACCEPT] = 1;
        strcat(requestHeader, accept1);
    }
    else if (!strcmp("Accept-Encoding", index)) {
        flags[ACCEPT_ENCODING] = 1;
        strcat(requestHeader, accept_encoding);
    }
    else if (!strcmp("Connection", index)) {
        flags[CONNECTION] = 1;
        strcat(requestHeader, connection);
    }
    else if (!strcmp("Proxy-Connection", index)) {
        flags[PROXY_CONNECTION] = 1;
        strcat(requestHeader, proxy_connection);
    }
    else  {
        strcat(requestHeader, buf1);
    }
    return flags;
}

void *ajust_request_header(int *flags, char *hostname, char *requestHeader)
{
    char buf1[MAXLINE];

    memset(buf1, 0, sizeof(buf1));

    if (!flags[HOST]) {
        strcat(buf1, "Host: ");
        strcat(buf1, hostname);
        strcat(buf1, "\r\n");
        strcat(requestHeader, buf1);
    }
    if (!flags[USER_AGENT]) {
        strcat(requestHeader, user_agent);
    }
    if (!flags[ACCEPT]) {
        strcat(requestHeader, accept1);
    }
    if (!flags[ACCEPT_ENCODING]) {
        strcat(requestHeader, accept_encoding);
    }
    if (!flags[CONNECTION]) {
        strcat(requestHeader, connection);
    }
    if (!flags[PROXY_CONNECTION]) {
        strcat(requestHeader, proxy_connection);
    }
    return requestHeader;
}

unsigned int getHostname(char *uri) 
{
	char *p = uri;
	unsigned int count = 0;
	unsigned int count1 = 0;

    printf("uri = %s\n", uri);

	while (*p != '\0') {
		if (count1 == 3) {
			printf("count = %d\n", count);
            return (count - 1); 
        }
		if (*p == '/') {
			count1++;
		}
		count++;
		p++;
	}
	return (count - 1);
}


// bool validArg(char *p)
// {
// 	bool flag = true;
// 	while ((*p != '\r') && (*p != '\n')) {
// 		if ((*p > '9') || (*p < '0')) {
// 			flag = false;
// 			break;
// 		}
// 		p++;
// 	}
// 	return flag;
// }

void *getIndex(char *p, char *index)
{
	index = memcpy(index, p, (strchr(p, ':') - p));
	return (void *)index;
}







