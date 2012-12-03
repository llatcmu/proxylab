#include <stdio.h>
#include "csapp.h"
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 4
#define SBUFSIZE 16


static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_msg = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";

static sem_t mutex;
sem_t sem_adapt_LRU;
sem_t sem_total_cache_size;
pthread_rwlock_t rwlock;
struct line
{
    int valid;
    char* tag;
    unsigned int len;
    char* block;
    struct line* ptr;
};
struct line* my_line = NULL;
unsigned int Total_cache_size = 0;

struct line* get_cache_data(char* uri,char* buf);
void set_cache_data(char* uri,char* buf,unsigned int datasize);
void Adapt_LRU(struct line* sign);
void doit(int fd);
void *thread(void *vargp);
int parse_uri(char* uri, char* host, char* path, unsigned short port);
char *parse_header(char *header);
int open_clientfd_multi(char *hostname, int port);

int main(int argc, char **argv)
{
    int i, listenfd, *connfd, port;
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(struct sockaddr_in); 
    pthread_t tid;
    pthread_rwlock_init(&rwlock,NULL);
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    if (strlen(argv[1]) > 5) {
        fprintf(stderr, "Error: port invalid\n");
        exit(0);
    }
    for (i = 0; i < strlen(argv[1]); i++) {
        if (argv[1][i] > 57 || argv[1][i] < 48) {
            fprintf(stderr, "Error: port invalid\n");
            exit(0);
        }
    }
    port = atoi(argv[1]);
    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(port);
    sem_init(&mutex, 0, 1);
    sem_init(&sem_adapt_LRU,0,1);
    sem_init(&sem_total_cache_size,0,1);

    while (1) {
        connfd = malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        printf("client address: %s\nclient port: %u\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    return 0;
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int fd)
{
    int n = 0;
    int clientfd;
    unsigned short port = 80;
    char host[MAXLINE] = {0}, path[MAXLINE] = {0};
    char method[MAXLINE] = {0}, uri[MAXLINE] = {0}, version[MAXLINE] = {0}, header[MAX_OBJECT_SIZE] = {0};
    char buf1[MAX_OBJECT_SIZE] = {0}, buf2[MAX_OBJECT_SIZE] = {0};
    rio_t rio1, rio2;
    unsigned int datasize = 0;
    struct line* sign;

    memset(header, 0, sizeof(header));
    memset(buf1, 0, sizeof(buf1));
    memset(buf2, 0, sizeof(buf2));
    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));
    memset(method, 0, sizeof(method));
    memset(uri, 0, sizeof(uri));
    memset(version, 0, sizeof(version));

    rio_readinitb(&rio1, fd);
    if (rio_readlineb(&rio1, buf1, MAXLINE) <= 0) return;
    sscanf(buf1, "%s %s %s", method, uri, version); 
    if (strcasecmp(method, "GET") != 0) {
        printf("Sorry, we only support GET method.\n");
        return;
    }
    parse_uri(uri, host, path, port);    
    memset(buf1, 0, sizeof(buf1));   

    pthread_rwlock_rdlock(&rwlock);
    sign = get_cache_data(uri,buf1);
    pthread_rwlock_unlock(&rwlock);

    if(sign != NULL) {
        printf("Get content from cache !!\n");
        P(&sem_adapt_LRU);
        Adapt_LRU(sign);
        V(&sem_adapt_LRU);
        if ((n = rio_writen(fd,buf1,sizeof(buf1))) < 0) {
            fprintf(stderr, "rio_writen error: send data to client from cache failed!\n");
            return;
        }
        memset(buf1,0,sizeof(buf1));
        return;
    }
    else {
        printf("Get content from real server!\n");
    	while ((n = rio_readlineb(&rio1, buf1, MAXLINE)) > 0) { 
            if (!strcmp(buf1, "\r\n")) break;
            sprintf(header, "%s%s", header, parse_header(buf1));
            memset(buf1, 0, sizeof(buf1));
   	}
        if (n < 0) {
	    fprintf(stderr, " rio_read error: read request from client failed!\n");
            return;
        }
    	memset(buf1, 0, sizeof(buf1));

    	printf("uri: %s\nhost: %s\nparth: %s\nport:%u\nheader:%s", uri, host, path, port, header);
    	if ((clientfd = open_clientfd_multi(host, port)) < 0) {
            fprintf(stderr, "Connect server error!\n");
            return;
    	}
    	rio_readinitb(&rio2, clientfd);

    	sprintf(buf1, "GET %s HTTP/1.0\r\nHost: %s\r\n%s%s%s", path, host, user_agent, accept_msg, accept_encoding);
    	sprintf(buf1, "%s%s%s%s\r\n", buf1, connection, proxy_connection, header);
    	printf("buf1:\n%s", buf1);
    	if (rio_writen(clientfd, buf1, strlen(buf1)) < 0) {
            fprintf(stderr, "rio_writen error: fail to send request！\n");
            close(clientfd);
            return;
    	}
    	memset(buf1, 0, sizeof(buf1));

        while ((n = rio_readnb(&rio2, buf2, MAX_OBJECT_SIZE)) > 0) {
            if (n < MAX_OBJECT_SIZE) {
                datasize += n;
                memcpy(buf1,buf2,n);
                if (rio_writen(fd, buf2, n) < 0) {
                    fprintf(stderr, "rio_writen error: fail to send response！\n");
                    close(clientfd);
                    return; 
                }
                memset(buf2, 0, sizeof(buf2));
            } 
            else {      
                datasize += n;
                if (rio_writen(fd, buf2, n) < 0) {
                     fprintf(stderr, "rio_writen error: fail to send response！\n");
                     close(clientfd);
                     return; 
           	}
            	memset(buf2, 0, sizeof(buf2));
            }
        }
        if (n < 0) {
            fprintf(stderr, "rio_readnb error: faile to read response from server!\n");
            return;
        }
        if (datasize < MAX_OBJECT_SIZE) {
            printf("Enter to write cache\n");
            pthread_rwlock_wrlock(&rwlock);
            printf("Enter to write\n");
            printf("buf3 is %s\nend buf3",buf1);
            set_cache_data(uri,buf1,datasize);
            pthread_rwlock_unlock(&rwlock);
            printf("Write end\n");
        }
        datasize = 0;
        memset(buf1,0,sizeof(buf1));
    	Close(clientfd);
    }
    printf("Connection to one client closed\n");
}

void Adapt_LRU(struct line* sign)
{
    struct line* tmp = my_line;
    if(tmp == sign) return;
    else
    {
   	 while(tmp->ptr != sign)
   	 {
            if(tmp->ptr == NULL) printf("Can not be possible!!!!\n"); 
            tmp = tmp->ptr;
         }
    
         tmp->ptr = (tmp->ptr) -> ptr;
         sign -> ptr = my_line;
         my_line = sign;
    }
}

void set_cache_data(char* uri,char* buf,unsigned int datasize)
{
    struct line* tmp_line = malloc(sizeof(struct line));
    struct line* traverse_ptr = NULL;
    struct line* pre_ptr = NULL;
    unsigned int buf_size = datasize;
    tmp_line -> valid = 1;
    tmp_line -> len = buf_size;
    tmp_line -> tag = malloc(MAXLINE);
    strcpy(tmp_line ->tag,uri);
    tmp_line -> block = malloc(buf_size);
    memcpy(tmp_line-> block,buf,buf_size);
    tmp_line -> ptr = NULL;
    P(&sem_total_cache_size);
    if(Total_cache_size + buf_size < MAX_CACHE_SIZE)
    {
        if(my_line == NULL)
        {
            my_line = tmp_line;
        }
        else
        {
            tmp_line -> ptr = my_line;
            my_line = tmp_line;
        }
        Total_cache_size += buf_size;
    }
    else
    {
        tmp_line -> ptr = my_line;
        my_line = tmp_line;
        Total_cache_size = Total_cache_size + buf_size;
        while(Total_cache_size > MAX_CACHE_SIZE)
        {
            traverse_ptr = my_line;
            while(traverse_ptr->ptr != NULL)
            {
                pre_ptr = traverse_ptr;
                traverse_ptr = traverse_ptr->ptr; 
            }
            pre_ptr -> ptr = NULL;
            Total_cache_size = Total_cache_size - traverse_ptr->len;
            free(traverse_ptr -> tag);
            free(traverse_ptr -> block);
            free(traverse_ptr);
        }
    }
    V(&sem_total_cache_size);
}

struct line* get_cache_data(char* uri,char* buf)
{
    struct line* traverse_ptr = my_line;
    while(traverse_ptr != NULL)
    {
        if((traverse_ptr->valid == 1) && (strcmp(traverse_ptr->tag,uri) == 0))
        {
            printf("IN function get traverse_ptr->block is %s\n",traverse_ptr->block);
            memcpy(buf,traverse_ptr->block,traverse_ptr->len);
            break;
        }
        traverse_ptr = traverse_ptr -> ptr;
    }
    return traverse_ptr;
}

int open_clientfd_multi(char *hostname, int port) 
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    P(&mutex);
    printf("before gethostbyname...\n");
    if ((hp = gethostbyname(hostname)) == NULL) {
        V(&mutex);
	return -2; /* check h_errno for cause of error */
    }
    printf("after gethostbyname...\n");
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



int parse_uri(char* uri, char* host, char* path, unsigned short port)
{
    int i = 0, p = 0, h = 0, p_hd = 0;
    char tmp;
    char newport[MAXLINE];
    for (i = 0, h = 0, p = 0, p_hd = 0; (tmp = *(uri+i)) != '\0'; i++) {
        if (tmp == ':' && i > 6) { 
            p_hd = i;
        }
        else if (i > 6 && tmp == '/') {
            p = i;
            break;
        }
        else if (i > 6 && !p_hd) { 
            *(host + h) = *(uri + i);
            h++;
        }
    }
    for (i = 0; (tmp = *(uri+p)) != '\0'; p++, i++) {
        *(path + i) = tmp;
    }
    if (p_hd == 0)
        port = 80;
    else {
        for ( i = 0; (tmp = *(uri + p_hd)) != '/' && tmp != '\0'; i++, p_hd++)
            *(newport + i) = tmp;
        port = atoi(newport);
    }
    return 0;
}

char *parse_header(char *header) 
{
    char tmp;
    char tmp_header[MAXLINE] = {0};
    int i = 0, j = 0;
    while ((tmp = *(header + i)) != ':' && tmp != '\0') {
        *(tmp_header + j) = tmp;
        j++;
        i++;
    }
    if (strcasecmp(tmp_header, "Host") && strcasecmp(tmp_header,"User-Agent") && strcasecmp(tmp_header,"Accept") && \
           strcasecmp(tmp_header,"Accept-Encoding") && strcasecmp(tmp_header,"Connection") && strcasecmp(tmp_header,"Proxy-Connection"))
       return header;
    memset(header, 0, sizeof(header));
    return header;
}
