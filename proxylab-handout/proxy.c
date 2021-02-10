#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

#include "csapp.h"

void doit(int fd);
void build_requestline(char *buf, char *method, char *newuri, char *version, int *port, char *new_requestline);
void build_requesthdr(rio_t *rp, int connfd, char *new_requestline, char *new_request);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);


int Open_endserverfd(char *host, int port);

int Open_endserverfd(char *host, int port){

    char portstr[100];
    sprintf(portstr, "%d", port);
    return Open_clientfd(host, portstr);
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    printf("%s", user_agent_hdr);

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             //line:netp:tiny:doit
	Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int clientfd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], newuri[MAXLINE], version[MAXLINE], new_requestline[MAXLINE], new_request[MAXLINE];
    int port=80;//default port
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    //request line needs parsed
    // GET http://www.cmu.edu:8080/hub/index.html HTTP/1.1
    // is parsed into....
    // GET /hub/index.html HTTP/1.0, and port:8080
    Rio_readinitb(&rio, clientfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    //printf("%s", buf);
    //sscanf(buf, "%s %s %s", method, newuri, version);       //line:netp:doit:parserequest
                                                       //line:netp:doit:endrequesterr
    build_requestline(buf, method, newuri, version, &port, new_requestline);
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(clientfd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    } 
    build_requesthdr(&rio, clientfd, new_requestline, new_request);                              //line:netp:doit:readrequesthdrs
    printf("--request shown below will be sent to the endserver\n%s",new_request);
    Rio_writen(clientfd, new_request, strlen(new_request));

    int endserverfd=Open_endserverfd(newuri, port);
    printf("--endserverfd,,%d", endserverfd);
    rio_t rio_endserv;
    Rio_readinitb(&rio_endserv, endserverfd);
    Rio_writen(endserverfd, new_request, strlen(new_request));
    int n;
    while ((n = Rio_readlineb(&rio_endserv, buf, MAXLINE)) != 0) {
        printf("--from endserver,, %s", buf);
    }
    Close(endserverfd); 

   
}
/* $end doit */

void build_requestline(char *buf, char *method, char *newuri, char *version, int *port, char *new_requestline)
{
    char olduri[MAXLINE];
    char filepath[MAXLINE];

    sscanf(buf,"%s %s %s\r\n", method, olduri, version);
    // same as...
    //sscanf(buf, "%s %s %s", method, newuri, version);
    printf("--%s,, %s,, %s\n", method, olduri, version);

    // build new uri

    // acceptable uri formats
    //http://....
    //www.twitter.com:8080/index.html
    //www.twitter.com/index.html
    //www.twitter.com:8080
    //www.twitter.com

    // ptr1//, ptr2:, ptr3/

    char *ptr1=strstr(olduri,"//");
    if(ptr1!=NULL){
        ptr1=ptr1+2;
    }else
        ptr1=olduri;

    char *ptr2=strstr(ptr1,":");
    if(ptr2!=NULL){//www.twitter.com:8080/index.html, www.twitter.com:8080
        *ptr2='\0';
        //sscanf(ptr1,"%s",newuri);
        strcpy(newuri, ptr1);
        sscanf(ptr2+1,"%d%s", port, filepath);   
    }else{
        char *ptr3=strstr(ptr1,"/");
        if(ptr3!=NULL){//www.twitter.com/index.html
            strcpy(filepath,ptr3);
            *ptr3='\0';
            strcpy(newuri,ptr1);
        }else{//www.twitter.com
            strcpy(filepath, "");
            strcpy(newuri,olduri);
        }
    }

    printf("newuri,,%s\n", newuri);
    printf("port,,%d\n", *port);
    printf("filepath,,%s\n", filepath);

    sprintf(new_requestline, "%s %s%s %s\r\n", method, newuri, filepath, version);
 
}
/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void build_requesthdr(rio_t *rp, int connfd, char *new_requestline, char *new_request) 
{
    size_t n;
    char buf[MAXLINE];
    char other_hdr[MAXLINE];
    char *connection_key="Conection";
    char *proxy_connection_key="Proxy-Connection";
    char *user_agent_key="User-Agent";

    while ((n = Rio_readlineb(rp, buf, MAXLINE)) != 0) {

        // end of request header
        if (strcmp(buf, "\r\n") == 0)
            break;

        if(strstr(buf,"Host")!=NULL){
            printf("--%s\n", buf);
        }else if(strstr(buf, "Connection")!=NULL){
            printf("--%s\n", buf);
        }else if(strstr(buf, "Proxy-connection")!=NULL){
            printf("--%s\n", buf);
        }else if(strstr(buf, "User-Agent")!=NULL){
            printf("--%s\n", buf);
        }else{
            printf("%s", buf);
            //char temp[MAXLINE];
            //sscanf(buf, "%s\r\n", temp);
            //sprintf(other_hdrs,"%s%s", other_hdrs, temp);
            sprintf(other_hdr,"%s%s", other_hdr, buf); // sprintf copies including \r\n at the tail as well
        }
  }

  sprintf(new_request, "%s%s%s%s%s\r\n",
    new_requestline,
    user_agent_hdr,
    "Conection: close\r\n",
    "Proxy-Connection: close\r\n",
    other_hdr
  );

}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
	strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
	strcat(filename, uri);                           //line:netp:parseuri:endconvert1
	if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
	return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	}
	else 
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	return 0;
    }
}
/* $end parse_uri */



/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    Close(srcfd);                       //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "%sContent-type: text/html\r\n\r\n", buf);
    /* Print the HTTP response body */
    sprintf(buf, "%s<html><title>Tiny Error</title>", buf);
    sprintf(buf, "%s<body bgcolor=""ffffff"">\r\n", buf);
    sprintf(buf, "%s%s: %s\r\n", buf, errnum, shortmsg);
    sprintf(buf, "%s<p>%s: %s\r\n", buf, longmsg, cause);
    sprintf(buf, "%s<hr><em>The Tiny Web server</em>\r\n", buf);
    Rio_writen(fd, buf, strlen(buf));

}
/* $end clienterror */
