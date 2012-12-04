#include <stdio.h>
#include "string.h"
#include "csapp.h"
#include "pcache.h"

#define MAX_CACHE_SIZE  1049000
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

//Sorry I cannot get this line under 80 characters
static const char *user_agent = 
"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

static const char *accept1 = 
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";

static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";
static const char *get = "GET ";
static const char *version = " HTTP/1.0\r\n";
static void *uri_error = "the request is not a valid URI\r\n";
static void *method_error = "We only can deal with GET method.\r\n";

sem_t mutex;
//get_set_rwlock is a read-write lock used when
//concurrent threads are trying to read from and write to
//the cache
pthread_rwlock_t get_set_rwlock;

//search_update_rwlock is also a read-write lock used when
//concurrent threads are searching and then updating
//the cache
pthread_rwlock_t search_update_rwlock;

void int_handler(int sig);
int validArg(char *p);
void *thread(void *vargp);
unsigned int getHostname(char *uri);
void *getIndex(char *p, char *index);
void doit(int fd);
int open_clientfd_with_mutex(char *hostname, int port);
int *generate_request_header(char *requestHeader, char *buf1, int *flags);
void *ajust_request_header(int *flags, char *hostname, char *requestHeader);
void *generate_request(char *request, char *uri, char *method, 
                        char *hostname, int *serverport); 


/*
 * main
 *
 * Initialize file descriptors and all threads whenever there's a request
 */
int main(int argc, char **argv) 
{   

    int listenfd, port, *connfdp;
    pthread_t tid;
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);

    if (argc != 2) {
        printf("Argument is invalid.\n");
        exit(0);
    }
    if (!validArg(argv[1])) {
        printf("Argument is invalid.\n");
        exit(0);
    }

    port = atoi(argv[1]);

    if ((port < 1000) || (port > 64000)) {
        printf("Port Number is invalid\n");
        exit(0);
    }

    Signal(SIGPIPE, SIG_IGN);
    Signal(SIGINT, int_handler);
    listenfd = Open_listenfd(port);
    sem_init(&mutex, 0, 1);
    pthread_rwlock_init(&get_set_rwlock, NULL);
    pthread_rwlock_init(&search_update_rwlock, NULL);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    return 0;
}

/*
 * thread
 *
 * Helper method used to determine the life cycle of a thread
 */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

/*
 * doit
 *  - used the name from the textbook
 * 
 * Main function for proxy behaviors
 */
void doit(int fd) 
{
    rio_t rio1, rio2;
    char buf1[MAXLINE] = {0}, buf2[MAX_OBJECT_SIZE] = {0}; 
    char method[MAXLINE] = {0}, uri[MAXLINE] = {0};
    char hostname[MAXLINE] = {0}, request[MAX_OBJECT_SIZE] = {0};
    char requestHeader[MAX_OBJECT_SIZE], cacheBuffer[MAX_OBJECT_SIZE];
    char *cachePoint;
    char check_uri[7];   
    linePCache *cacheLine;
    
    int datasize = 0;
    int n, clientfd, serverport = 80;
    int  flags[6] = {0, 0, 0, 0, 0, 0};

    rio_readinitb(&rio1, fd);    
    if (rio_readlineb(&rio1, buf1, MAXLINE) <= 0) {
        printf("Null request\n");
        return;
    }
        
    sscanf(buf1, "%s %s", method, uri);
    if (strcmp(method, "GET")) {
        if (rio_writen(fd, method_error, strlen(method_error)) < 0) {
            printf("rio_writen error.\n");
            return;
        }
        printf("We only can deal with GET method.\n");
        return;
    }
    if (strcmp(strncpy(check_uri, uri, 7), "http://")) {
        if (rio_writen(fd, uri_error, strlen(uri_error)) < 0) {
            printf("rio_writen error\n");
            return;
        }
        printf("the request is not a valid URI\n");
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

    //Protect the reading process so that multiple
    //threads can read from cache simutaniously
    pthread_rwlock_rdlock(&get_set_rwlock);

    //Search the cache to see if the request is cached
    //This process can be concurrent
    //But has to be separated with updating LRU
    //Therefore protected by a read lock
    pthread_rwlock_rdlock(&search_update_rwlock);
    cacheLine = get_webobj_from(uri);
    pthread_rwlock_unlock(&search_update_rwlock);

    if (cacheLine != NULL) {
        //Update cache sequence (write operation)
        //Has to be locked from searching
        //Therefore protected by a write lock
        pthread_rwlock_wrlock(&search_update_rwlock);
        update_cache(cacheLine);
        pthread_rwlock_unlock(&search_update_rwlock);

        //Get pointer to data from cacheline
        cachePoint = cacheLine->webobj_buf;   
        //Write data to client  
        if ((n = rio_writen(fd, cachePoint, cacheLine->obj_length)) < 0) {
            printf("rio_writen error, can not send data to"  
                "client from cache\n");
            //Abnormal aborts, unlock the read lock
            pthread_rwlock_unlock(&get_set_rwlock);
            return;
        }  

        //Finished reading from cache
        //Unlock the read lock
        pthread_rwlock_unlock(&get_set_rwlock); 
        return;
    }
    else {
        //Not found in the cache
        //So unlock the read lock
        pthread_rwlock_unlock(&get_set_rwlock);

        if ((clientfd = open_clientfd_with_mutex(hostname, serverport)) < 0) 
        {
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

        while ((n = rio_readnb(&rio2, buf2, MAX_OBJECT_SIZE)) > 0) {
            memcpy(cacheBuffer, buf2, n);
            
            if (rio_writen(fd, buf2, n) < 0) {
                printf("the error occured in rio_writen\n");
                Close(clientfd);
                return;
            }          
            memset(buf2, 0, sizeof(buf2));   
            datasize += n;    
        }

        //When datasize is smaller than our max web object size
        //We insert the object into the cache
        if (datasize < MAX_OBJECT_SIZE) {
            //The writing process has to be serialized
            //Therefore protected by a write lock
            
            //Also note that racing conditions can be effectively
            //Prevented because when writing to cache
            //The write lock makes sure that no one is trying to
            //Update cache for LRU purposes
            pthread_rwlock_wrlock(&get_set_rwlock);
            set_webobj_to(uri, cacheBuffer, datasize);
            pthread_rwlock_unlock(&get_set_rwlock);
        }
      
        Close(clientfd);
    }    


}

/* 
 * Besides the original, this modified function is just to 
 * make sure the gethostbyname() function is thread safe.
 */ 
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

/* 
 * This function is to parse the uri to generate the request to 
 * the server.
 */
void *generate_request(char *request, char *uri, char *method, 
                        char *hostname, int *serverport) 
{
    char uriForward[MAXLINE];
    char *p; 

    // parse to get the hostname
    strncpy(hostname, uri, getHostname(uri));
    p = memchr(hostname, '/', strlen(hostname));
    p = memchr(p + 1, '/', strlen(hostname));
    strcpy(hostname, p + 1);

    // if the optional field port is exist, we need change the port number.
    if ((p = memchr(hostname, ':', strlen(hostname))) != NULL) {
        *serverport = atoi(p + 1);
        strtok(hostname, ":");
    }

    strcpy(uriForward, uri + getHostname(uri));

    printf("hostname = %s, serverport = %d, uriForward = %s\n",
             hostname, *serverport, uriForward);

    // merge the necessary parts to generate the request.
    memset(request, 0, sizeof(request));
    strcat(request, get);
    strcat(request, uriForward);
    strcat(request, version);

    return request;
}

 /*
  * This helper function is to generate the request header,
  * if some specified fields are exist in the header, we need 
  * to replace it, otherwise, we just append it in the end of 
  * the header.
  */
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

/*
 * This helper function is to ajust the request header which be 
 * forwarded to server, to make sure the header include all the 
 * necessary parts. 
 */
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

/*
 * This helper function is to find the end position of 
 * hostname in uri.
 */
unsigned int getHostname(char *uri) 
{
	char *p = uri;
	unsigned int count = 0;
	unsigned int count1 = 0;

	while (*p != '\0') {
		if (count1 == 3) {
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

/*
 * This helper function to determine the port number 
 * is valid.
 */   
int validArg(char *p)
{
	int flag = 1;
    int i = 0;
	while (i < strlen(p)) {
		if ((*p > '9') || (*p < '0')) {
			flag = 0;
			break;
		}
		p++;
	}
	return flag;
}

/*
 * This helper function is to parse the 
 * description of every request header.
 */
void *getIndex(char *p, char *index)
{
	index = memcpy(index, p, (strchr(p, ':') - p));
	return (void *)index;
}

void int_handler(int sig) 
{
    printf("Exit\n");
    free_cache();
    exit(0);
}






