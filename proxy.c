#include <stdio.h>
#include "string.h"
#include "csapp.h"
#include "pcache.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define HOST             0
#define USER_AGENT       1
#define ACCEPT           2
#define ACCEPT_ENCODING  3
#define CONNECTION       4
#define PROXY_CONNECTION 5

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept1 = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";

static sem_t mutex;


unsigned int getPacketLength(char *buf2);
unsigned int getHostname(char *uri);
void *getIndex(char *p, char *index);
void doit(int fd);
int open_clientfd_with_mutex(char *hostname, int port) ;
void reinitial(char *buf1, char *buf2, char *method, char *uri, char *uriForward,
                char *hostname, char *request, char *index, char *requestHeader);


void reinitial(char *buf1, char *buf2, char *method, char *uri, char *uriForward,
                char *hostname, char *request, char *index, char *requestHeader)
{
    memset(buf1, 0, strlen(buf1));
    memset(buf2, 0, strlen(buf2));
    memset(method, 0, strlen(method));
    memset(uri, 0, strlen(uri));
    memset(uriForward, 0, strlen(uriForward));
    memset(hostname, 0, strlen(hostname));
    memset(request, 0, strlen(request));
    memset(index, 0, strlen(index));
    memset(requestHeader, 0, strlen(requestHeader));
    return;

}

// <<<<<<< HEAD
// unsigned int getPacketLength(char *buf2) 
// {
//     char index[MAXLINE] = {0};
//     char *length;
//     getIndex(buf2, index);
//     if (!strcmp(index, "Content-Length")) {
//         strchr(buf2, ':', strlen(buf2));
//         strcpy(length, buf2 + 2);
//         return atoi(length);
//     }
//     else
//         return 0;
// }
// =======
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


void doit(int fd) 
{
	rio_t rio1, rio2;
	char buf1[MAXLINE] = {0}, buf2[MAX_OBJECT_SIZE] = {0}, method[MAXLINE] = {0}, uri[MAXLINE] = {0};
	char uriForward[MAXLINE] = {0}, hostname[MAXLINE] = {0};
	char request[MAXLINE] = {0}, index[MAXLINE] = {0};
	char requestHeader[MAX_OBJECT_SIZE];
    char *p, *cachePoint, *cacheBuffer;
	int n, clientfd, serverport = 80;
	int flags[6] = {0, 0, 0, 0, 0, 0};
	const char *get = "GET ";
	const char *version = " HTTP/1.0\r\n";

	rio_readinitb(&rio1, fd);
	
    if (rio_readlineb(&rio1, buf1, MAXLINE) <= 0)
        return;

	sscanf(buf1, "%s %s", method, uri);
    if (strcmp(method, "GET")) {
        printf("We only can deal with GET method.\n");
        return;
    }

    if (get_webobj_from(uri, cacheBuffer) != NULL) {
        if ((rio_writen(fd, cacheBuffer, strlen(cacheBuffer))) < 0) {
            printf("written error.\n");
            return;
        }
        memset(cacheBuffer, 0, sizeof(cacheBuffer));
        return;
    }

	strncpy(hostname, uri, getHostname(uri));

    p = memchr(hostname, '/', strlen(hostname));
    p = memchr(p + 1, '/', strlen(hostname));
    strcpy(hostname, p + 1);
    if ((p = memchr(hostname, ':', strlen(hostname))) != NULL) {
        serverport = atoi(p + 1);
        strtok(hostname, ":");
    }

	strcpy(uriForward, uri + getHostname(uri));

    printf("hostname = %s, serverport = %d, length = %d, uriForward = %s\n", hostname, serverport, (int)strlen(hostname), uriForward);

    memset(request, 0, sizeof(request));
    strcat(request, get);
	strcat(request, uriForward);
	strcat(request, version);

    printf("request = %s\n", request);

	memset(buf1, 0, sizeof(buf1));
	if ((n = rio_readlineb(&rio1, buf1, MAXLINE)) < 0) {
        return;
    }

    while (strcmp(buf1, "\r\n")) {

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

    	memset(buf1, 0, sizeof(buf1));
    	if ((n = rio_readlineb(&rio1, buf1, MAXLINE)) < 0 )
            return;
    }

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

    printf("%s%s\n", request, requestHeader);

	if ((clientfd = open_clientfd_with_mutex(hostname, serverport)) < 0) {
        printf("request uri error, cannot connect to the server\n");
        return;
    }


    printf("clientfd = %d\n", clientfd);

    strcat(requestHeader, "\r\n");

	if ((n = rio_writen(clientfd, request, strlen(request))) < 0) {
        Close(clientfd);
        return;
    }

    if ((n = rio_writen(clientfd, requestHeader, strlen(requestHeader))) < 0) {
        Close(clientfd);
        return;
    }

// <<<<<<< HEAD
//     char *tmpWebObject;
//     tmpWebObject = Malloc(MAX_OBJECT_SIZE);

//     while ((n = rio_readlineb(&rio2, buf2, MAXLINE)) > 0) {
//         rio_writen(fd, buf2, n);
//         length = getPacketLength(buf2);
        
//         if (length > MAX_OBJECT_SIZE) {
//             should_discard_obj = 1;
//         }
//         else if (length != 0) {
//             packetLength = length;
//         }

//         if (!should_discard_obj) {
//             tmpWebObject = stpcpy(tmpWebObject, buf2);
//         }
//         memset(buf2, 0, sizeof(buf2));        
//     }
    

//     if (should_discard_obj)
//     {
//         /* nothing */
//     }
//     else {

//     }


//     if (n < 0)
// =======
    rio_readinitb(&rio2, clientfd);
     
    memset(buf2, 0, sizeof(buf2));
    while ((n = rio_readnb(&rio2, buf2, MAX_OBJECT_SIZE)) > 0) {
    	rio_writen(fd, buf2, n);
        memset(buf2, 0, sizeof(buf2));   
    }
    if (n < 0) {
        Close(clientfd);
        return;
    }        

    Close(clientfd);

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

    printf("port = %d\n", port);
<<<<<<< HEAD
	listenfd = Open_listenfd(port);
    signal(SIGPIPE, SIG_IGN);
=======

    Signal(SIGPIPE, SIG_IGN);
	listenfd = Open_listenfd(port);
    sem_init(&mutex, 0, 1);
>>>>>>> old
	while (1) {
		clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
	}
    return 0;
}
