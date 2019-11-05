#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h>
#include<string.h>
#include"head.h"
#include<stdio.h>
#include<stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include<errno.h>

int checkPort(char *buf) {

    while (*buf >= '0' && *buf <= '9') {
        ++buf;
    }

    if (*buf == '\0')
        return 1;
    else
        return 0;

}

void service_init(int argc, char *args[], service *se) {

    if (argc < 1) {
        printf("please specified the port\n");
        printUsage();
        exit(0);
    }
    /*
     if (strcmp(args[0], "-port") != 0)
     {
     printUsage();
     exit(0);
     }
     */
    if (!checkPort(args[0])) {
        printf("port number should between 1024 and 65535\n");
        printUsage();
        exit(0);
    }

    se->port = atol(args[0]);

    /*
     if (se->port < 1024 || se->port > 65535)
     {
     printf("port number should between 1024 and 65535\n");
     printUsage();
     exit(0);
     }*/

    se->tlist = (tnode *) malloc(sizeof(tnode));

    if (access("log", F_OK) < 0) {
        int logfd;
        if ((logfd = creat("log", 0666)) < 0) {

            printf("creat log error\n");
            exit(0);
        }
        close(logfd);

    }


    int fd = open("log", O_RDWR);

    if (fd < 0) {
        printf("open error\n");
        exit(0);
    }

    int newfd = dup2(fd, 2);
    if (newfd < 0) {
        printf("dup2 error\n");
        exit(0);
    }
    /*
     printf("%d\n", fd);
     printf("%d\n", newfd);*/

    se->tlist->fd = newfd;
    se->tlist->efd = fd;
    se->tlist->next = se->tlist;
    se->tlist->pre = se->tlist;

}

void service_destroy(service *se) {

    tnode *p = se->tlist;
    tnode *cur = p->next;
    while (cur != p) {
        p = cur->next;
        free(cur);
        cur = p;
    }
    free(p);
}

void printUsage() {
    printf("usage:./httpd   *[1024-65535]\n");
}

int createService(service *se) {

    struct sockaddr_in serveraddr;

    se->lsocket = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr, sizeof(struct sockaddr));

    serveraddr.sin_family = AF_INET;

    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    serveraddr.sin_port = htons(se->port);

    if (bind(se->lsocket, (struct sockaddr *) &serveraddr,
             sizeof(struct sockaddr_in)) < 0) {
        printf("bind error\n");
        exit(-1);
    } /*bind the address*/

    if (listen(se->lsocket, 128) < 0) {
        /*begin to listen*/
        printf("listen fail\n");
        exit(0);
    }

}

void *requestHandleThread(void *parg) {

    threadArg *arg = (threadArg *) parg;

    request re;

    initRequest(&re);
    /*init the request*/
    re.fd = arg->fd;
    re.efd = arg->efd;
    /*re.res.fd=arg->fd;*/
    /*
     printf("in the child\n");
     */
    getRequest(&re, arg->fd);
    /*get the request*/

    if (re.reclen <= 0) {
        /*
         printf("no data\n");*/
        close(arg->fd);
        /*do not receive info*/
        return 0 ;
    }

    getHead(&re, re.buf, re.reclen);

    /*
     printf("get the request\n");
     */
    getResponse(&re);
    printRequest(&re);
    /*
     */

    destroyRequest(&re);

    /* destroy the thread sync info*/

    pthread_mutex_t *pp = arg->lock;

    pthread_mutex_lock(pp);
    tnode *pre = arg->pre;
    tnode *cur = arg->next;

    pre->next = cur;
    cur->pre = pre;
    pthread_mutex_unlock(pp);

    close(arg->fd);
    free(arg);

}

void printThreadArgList(service *se) {
    tnode *p = se->tlist;
    tnode *cur = p->next;

    while (cur != p) {
        printf("fd:%d ", cur->fd);
        cur = cur->next;
    }
    printf("\n");
}

int handleRequest(service *se) {
    int cs;
    pthread_t th1;
    tnode *list = se->tlist;
    int result;
    pthread_mutex_t lock;
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("pthread init error\n");
        exit(0);
    }

    while ((cs = accept(se->lsocket, (struct sockaddr *) NULL, NULL)) > 0) {

        /*	printThreadArgList(se);	 */

        tnode *tp = (tnode *) malloc(sizeof(tnode));

        if (tp == NULL) {
            printf("malloc error\n");
            exit(0);
        }
        tp->fd = cs;
        tp->lock = &lock;
        tp->efd = list->fd;
        /*insert into the list*/

        pthread_mutex_lock(&lock);

        tp->next = list->next;
        list->next->pre = tp;

        list->next = tp;
        tp->pre = list;
        pthread_mutex_unlock(&lock);

        result = pthread_create(&th1, NULL, requestHandleThread, (void *) tp);

    }
}

void initRequest(request *re) {

    re->fd = 0;
    re->method = M_UNKNOWN;
    re->file = NULL;
    re->rpath = NULL;

    initResponse(&(re->res));

    re->buf = NULL;

    re->c.path = NULL;
    /*
     c->parms = NULL;*/
}

void initResponse(response *res) {

    strcpy(res->version, "HTTP/1.0");

    res->sstate = NULL;
    res->server = NULL;
    res->cdate = NULL;
    res->conten_type = NULL;

    res->content_length = 0;
    res->state = 404;

    /*res->path=NULL;
     */
}

void destroyResponse(response *res) {

    if (res->sstate != NULL) {
        free(res->sstate);
    }

    if (res->server != NULL) {
        free(res->server);
    }

    if (res->cdate != NULL) {
        free(res->cdate);
    }

    if (res->conten_type != NULL) {
        free(res->conten_type);
    }

}

void destroyRequest(request *re) {

    if (re->file != NULL) {
        free(re->file);
    }

    /*
     destroyResponse(&(re->res));
     */
    if (re->buf != NULL) {
        free(re->buf);
    }

    if (re->c.path != NULL) {
        free(re->c.path);
    }
}

void getRequest(request *re, int cs) {

    re->buf = (char *) malloc(sizeof(char) * SOCKETBUF_SIZE);
    if (re->buf == NULL) {
        printf("malloc error\n");
        exit(-1);
    }

    int retry = 8;

    while (retry--) {

        re->reclen = recv(cs, re->buf, SOCKETBUF_SIZE, MSG_DONTWAIT);
        if (re->reclen > 0)
            break;
        else {
//            sleep(1);
            /*
             long j = 0;
             while (j < 0xffff)
             {
             ++j;
             }*/
        }
    }

}

void getHead(request *re, char *buf, int len) {

    char *start, *end;

    char *lstart;
    char *p;
    start = buf;
    end = buf + len;

    while (start < end) {

        /*
         printf("***************************\n");
         */
        lstart = start;

        while (start < end && *start != '\n') {
            ++start;
        }

        /*
         p = lstart;
         while (p < start)
         {

         printf("%c", *p);
         ++p;
         }
         printf("\n");

         */
        break;

    }

    if (lstart >= start) {
        printf("error\n");
        exit(0);
    }

    char *pp;

    p = lstart;
    pp = lstart;

    while (pp < start && *pp != ' ')
        ++pp;

    if (p < pp)
        getMethod(re, p, pp - p);

    while (pp < start && *pp == ' ') {
        ++pp;
    }

    /* remove the space*/

    p = pp;
    while (pp < start && *pp != ' ')
        ++pp;

    getFilePath(re, p, pp - p);

}

void getMethod(request *re, char *from, int len) {

    char *buf = (char *) malloc(sizeof(char) * (len + 1));
    if (buf == NULL) {
        printf("malloc error\n");
        exit(0);
    }
    strncpy(buf, from, len);
    *(buf + len) = '\0';

    toLow(buf);
    /*
     printf("%s\n", buf);*/
    if (strcmp(buf, "get") == 0) {
        re->method = M_GET;
    } else if (strcmp(buf, "head") == 0) {
        re->method = M_HEAD;

    } else if (strcmp(buf, "post") == 0) {
        re->method = M_POST;
    } else if (strcmp(buf, "put") == 0) {
        re->method = M_PUT;
    } else if (strcmp(buf, "delete") == 0) {
        re->method = M_DELETE;
    } else {
        re->method = M_UNKNOWN;
    }
    /*
     printf("method:%s\n",buf);
     */
    free(buf);

}

void toLow(char *buf) {

    while (*buf != '\0') {
        if (*buf >= 'A' && *buf <= 'Z')
            *buf = *buf + 32;
        ++buf;
    }

}

void getFilePath(request *re, char *from, int len) {

    re->file = (char *) malloc(sizeof(char) * (len + 1));

    if (re->file == NULL) {

        printf("malloc error\n");
        exit(0);
    }

    strncpy(re->file, from, len);
    *(re->file + len) = '\0';

}

void printRequest(request *re) {
    printf("**********************\n");
    printf("fd:%d\n", re->fd);
    printf("method:%d\n", re->method);
    printf("solution:%d\n", re->solution);
    if (re->file) {
        printf("file:%s\n", re->file);
    }
    printf("**********************\n");
}

void getResponse(request *re) {

    checkFilePath(re);

    switch (re->solution) {

        case SOLU_BAD_REQUEST:
            /*
             printf("bad request\n");*/
            badRequestHandle(re);
            break;
        case SOLU_CGI:
            /*printf("cgihanler\n");*/
            cgiHandler(re);
            break;
        case SOLU_OTHER:

            if (re->method == M_GET) {
                gethandler(re);
            } else if (re->method == M_HEAD) {
                headHandler(re);
            } else {
                /*
                 printf("bad request\n");*/
                badRequestHandle(re);
            }
            break;
    }

}

void getUrlParm(request *re) {

    char *st, *end;
    char *index;
    cgi *c = &(re->c);
    st = re->file;

    if (*st == '/')
        ++st;

    end = re->file + strlen(re->file);

    index = st;
    while (*index != '?' && index < end) {
        ++index;
    }

    int len = index - st;

    c->path = (char *) malloc((len + 1) * sizeof(char));

    if (c->path == NULL) {

        printf("malloc error\n");
        exit(0);
    }

    strncpy(c->path, st, len);
    *(c->path + len) = '\0';

    if (index < end) {

        ++index;
        st = index;
        len = 0;
        while (index < end) {
            if (*index == '&') {
                ++len;
            }
            ++index;
        }

        /*
         c->num = len + 1;
         c->parms = (char **) malloc(sizeof(char *) * (len + 2));

         c->parms[len + 1] = NULL;
         */
        index = st;

        int i = 0;
        while (index < end) {
            st = index;
            while (*index != '&' && index < end) {
                ++index;
            }
            len = index - st;
            /*
             *(c->parms + i) = (char *) malloc(sizeof(char) * (len + 1));
             strncpy(*(c->parms + i), st, len);
             *(*(c->parms + i) + len) = '\0';*/
            ++index;
            ++i;
        }

    } else {

    }

}

void getCGIParm(request *re) {

    cgi *c = &(re->c);

    char *p = re->file;
    if (*p == '/')
        p++;
    c->path = p;

    while (*p != '\0') {

        if (*p == '?')
            *p = ' ';
        if (*p == '&') {
            *p = ' ';
        }

        ++p;
    }
}

void checkCGIPath(request *re) {

    cgi *c = &(re->c);

    int len = strlen(re->file);

    c->path = (char *) malloc(sizeof(char) * len);

    if (c->path == NULL) {

        printf("malloc error\n");
        exit(0);
    }

    char *from = re->file;
    if (*from == '/') {

        ++from;

    }
    char *p = c->path;

    while (*from != '\0' && *from != ' ') {
        *p = *from;
        ++p;
        ++from;

    }

    *p = '\0';
    /*
     printf("%s\n",c->path);
     */
    if (access(c->path, F_OK) < 0) {
        re->res.state = R_NOT_FOUND;
        /*not found*/
        return;
    }

    if (access(c->path, X_OK) < 0) {
        re->res.state = R_FORBIDDEN;
        return;
    }

    re->res.state = R_REQUEST_OK;

    *p = ' ';
    while (*from != '\0') {
        *p = *from;
        ++p;
        ++from;
    }
    *p = '\0';

}

void generateCGIHead(request *re) {

    response *ss = &(re->res);
    char *index = ss->buf;

    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, "HTTP/1.0 200 Ok\r\n");
            ss->length = 13 + 4;/*version state*/
            index += 17;
            break;
        case R_FORBIDDEN:
            strcpy(index, "HTTP/1.0 403 Permission Denied\r\n");
            ss->length = 13 + 19;/*version state*/
            index += 32;
            break;

        case R_NOT_FOUND:
            strcpy(index, "HTTP/1.0 404 NOT FOUND\r\n");
            ss->length = 13 + 11; /*version state*/
            index += 24;
            break;
        case R_ERROR:

            strcpy(index, "HTTP/1.0 500 Internal Error\r\n");
            ss->length = 13 + 16;/*version state*/
            index += 29;
            break;

    }
    strcpy(index, "Content-Type: text/html\r\n");
    index += 25;
    ss->length += 25;/*version state*/

    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, ss->slength);

            ss->length += strlen(ss->slength);/*version state*/
            index += strlen(ss->slength);
            break;
        case R_FORBIDDEN:
            strcpy(index, "Content-Length: 88\r\n\r\n");
            ss->length += 22;/*send:forbidden.*/
            index += 22;

            char
                    bb[] =
                    "<table> <head><title>not found</title></head> <body>403 Permission Denied</body></table>";
            strcpy(index, bb);
            /*printf("aa:%s %d\n", bb, strlen(bb));*/
            ss->length += strlen(bb);
            index += strlen(bb);
            *index = '\0';
            break;

        case R_NOT_FOUND:
            strcpy(index, "Content-Length: 80\r\n\r\n");
            ss->length += 22;/*send:not found.*/
            index += 22;

            char
                    aa[] =
                    "<table> <head><title>not found</title></head> <body>404 not found</body></table>";
            strcpy(index, aa);
            /*printf("aa:%s %d\n", aa, strlen(aa));*/
            ss->length += strlen(aa);
            index += strlen(aa);
            *index = '\0';
            break;
        case R_ERROR:
            strcpy(index, "Content-Length: 85\r\n\r\n");
            ss->length += 22;/*send:not found.*/
            index += 22;

            char
                    cc[] =
                    "<table> <head><title>not found</title></head> <body>500 Internal Error</body></table>";
            strcpy(index, cc);
            /*printf("aa:%s %d\n", cc, strlen(cc));*/
            ss->length += strlen(cc);
            index += strlen(cc);
            *index = '\0';
            break;

    }

}

void cgiErrorHandler(request *re) {
    response *ss = &(re->res);
    generateCGIHead(re);

    /*
    printf("cgierror:%s", ss->buf);
*/
    send(re->fd, ss->buf, ss->length, 0);

}

void cgiOKHandler(request *re) {

    char aa[2048];
    int length;
    struct stat st;
    response *ss = &(re->res);
    /*sprintf(aa, "before error: %s\n", strerror(errno));
     printf("popen error:%s", aa);*/

    /*
     int olen, nlen;
     fsync(re->efd);
     lstat("log", &st);

     olen = st.st_size;

     */

    /*
     lseek(re->efd, 0, SEEK_SET);
     if (ftruncate(re->efd, 0) < 0)
     {
     ss->state = R_ERROR;
     printf("here");
     cgiErrorHandler(re);
     return;
     }*/

    FILE *pin = popen(re->c.path, "r");

    /*	sprintf(aa, "after error: %s\n", strerror(errno));
     printf("popen error:%s", aa);
     */

    if (pin == NULL) {
        ss->state = R_ERROR;
        cgiErrorHandler(re);
        return;
    }
    /*
     printf("l log\n");
     fsync(re->efd);
     lstat("log", &st);
     nlen = st.st_size;
     printf(" %d %d get error\n", olen, nlen);

     if (olen != nlen)
     {
     ss->state = R_ERROR;
     cgiErrorHandler(re);
     return;

     }
     else
     {

     }*/
    /*
     printf("efd:%d\n", re->efd);*/
    /*
     lstat("log", &st);
     if (st.st_size > 0)
     {
     printf("get error");
     ss->state = R_ERROR;
     cgiErrorHandler(re);

     return;
     }else{
     printf("ok");
     }*/

    /* lseek(re->efd, 0, SEEK_SET);

     if ((length = read(re->efd, aa, 2048)) > 0)
     {
     printf("ok\n");
     ss->state = R_ERROR;
     cgiErrorHandler(re);
     return;
     }
     */
    sprintf(ss->slength, "temp%d.txt", re->fd);

    int tempfd = open(ss->slength, O_CREAT | O_RDWR | O_TRUNC, 0644);

    if (tempfd < 0) {
        ss->state = R_ERROR;
        cgiErrorHandler(re);
    }

    unlink(ss->slength);

    /*get the output from the info*/
    /*
     while (fgets(ss->buf, BUFFER_SIZE, pin) != NULL)
     {
     length = strlen(ss->buf);
     ss->content_length += length;
     write(tempfd, ss->buf, length);
     printf("content:%s", ss->buf);
     }
     */
    ss->content_length = 0;

    while ((length = fread(ss->buf, sizeof(char), BUFFER_SIZE, pin)) > 0) {
        ss->content_length += length;
        write(tempfd, ss->buf, length);
        /*
         printf("content:%s", ss->buf);*/
    }

    if (ss->content_length == 0) {

        ss->state = R_ERROR;
        cgiErrorHandler(re);
    }

    if (lseek(tempfd, 0, SEEK_SET) < 0) {
        ss->state = R_ERROR;
        cgiErrorHandler(re);
    }
    pclose(pin);

    /*from the begin to read*/

    sprintf(ss->slength, "Content-Length: %ld\r\n\r\n", ss->content_length);

    generateCGIHead(re);

    send(re->fd, ss->buf, ss->length, 0);
    /*printf("head:%s", ss->buf);*/

    while ((length = read(tempfd, ss->buf, BUFFER_SIZE)) > 0) {
        send(re->fd, ss->buf, length, 0);
    }
    close(tempfd);
}

void cgiNotFoundHandler(request *re) {
    response *ss = &(re->res);
    generateCGIHead(re);

    /*
     printf("%s", ss->buf);
     */
    send(re->fd, ss->buf, ss->length, 0);

}

void cgiForbidHandler(request *re) {
    response *ss = &(re->res);
    generateCGIHead(re);
    /*
     printf("%s", ss->buf);*/
    send(re->fd, ss->buf, ss->length, 0);

}

void cgiHandler(request *re) {
    response *ss = &(re->res);
    getCGIParm(re);

    checkCGIPath(re);
    /*
     printf("state:%d\n", ss->state);*/

    /*todo generate head*/

    switch (ss->state) {

        case R_REQUEST_OK:

            cgiOKHandler(re);
            break;

        case R_FORBIDDEN:

            cgiForbidHandler(re);
            break;

        case R_NOT_FOUND:

            cgiNotFoundHandler(re);
            break;
    }

}

void headHandler(request *re) {

    char *rpath = re->file;
    if (*rpath == '/') {
        ++rpath;
    }

    if (access(rpath, F_OK) < 0) {
        re->res.state = R_NOT_FOUND;
        /*printf("not found\n");*/
        notfoundHeadResponse(re);
        /*permition deny 404 not found */
    } else {
        if (access(rpath, R_OK) < 0) {
            re->res.state = R_FORBIDDEN;
            forbidHeadResponse(re);
            /*permition deny 403 forbid */
        } else {
            /*
             printf("here:head ok\n");*/
            re->res.state = R_REQUEST_OK;
            re->rpath = rpath;

            okHeadResponse(re);

            /*sendResponse(re);*/

        }

    }

}

void notfoundHeadResponse(request *re) {
    response *ss = &(re->res);
    getHeadResponseHead(re);

    /*
     printf("%s", ss->buf);*/

    send(re->fd, ss->buf, ss->length, 0);
}

void forbidHeadResponse(request *re) {
    response *ss = &(re->res);
    getHeadResponseHead(re);

    /*
     printf("%s", ss->buf);*/
    send(re->fd, ss->buf, ss->length, 0);

}

void okHeadResponse(request *re) {
    struct stat st;
    response *ss = &(re->res);
    if (lstat(re->rpath, &st) < 0) {
        printf("lstat error\n");
        exit(0);
    }

    ss->content_length = st.st_size;
    sprintf(ss->slength, "Content-Length: %ld\r\n\r\n", st.st_size);
    getResponseHead(re);

    /*
     printf("%s", re->buf);*/
    send(re->fd, ss->buf, ss->length, 0);

}

void gethandler(request *re) {

    char *rpath = re->file;
    if (*rpath == '/') {
        ++rpath;
    }

    char *end = rpath;
    while (*end != '\0' && *end != '?' && *end != ' ')
        ++end;

    char *p = re->file;
    while (rpath < end) {
        *p = *rpath;
        ++p;
        ++rpath;
    }
    *p = '\0';
    if (strlen(re->file) == 0) {
        strcpy(re->file, "index.html");
    }

    printf("%s\n", re->file);

    if (access(re->file, F_OK) < 0) {
        printf("kkk\n");
        re->res.state = R_NOT_FOUND;
        /*printf("not found\n");*/
        notfoundResponse(re);
        /*permition deny 404 not found */

    } else {
        if (access(re->file, R_OK) < 0) {
            re->res.state = R_FORBIDDEN;
            forbidResponse(re);
            /*permition deny 403 forbid */
        } else {
            re->res.state = R_REQUEST_OK;
            re->rpath = re->file;

            okResponse(re);

            /*sendResponse(re);*/

        }

    }

}

void forbidResponse(request *re) {
    response *ss = &(re->res);
    getResponseHead(re);

    /*
     printf("%s", ss->buf);*/
    send(re->fd, ss->buf, ss->length, 0);

}

void notfoundResponse(request *re) {
    response *ss = &(re->res);

    getResponseHead(re);

    /*
     printf("%s", ss->buf);*/

    send(re->fd, ss->buf, ss->length, 0);

}

void badRequestHandle(request *re) {
    response *ss = &(re->res);
    char *index = ss->buf;

    strcpy(index, "HTTP/1.0 400 Bad Request\r\n");
    ss->length = 26;/*version state*/
    index += 26;

    strcpy(index, "Content-Type: text/html\r\n");
    index += 25;
    ss->length += 25;/*version state*/

    strcpy(index, "Content-Length: 0\r\n\r\n");
    ss->length += 21;/*send:not found.*/
    index += 21;
    *index = '\0';

    send(re->fd, ss->buf, ss->length, 0);
}

void okResponse(request *re) {
    struct stat st;
    response *ss = &(re->res);
    if (lstat(re->rpath, &st) < 0) {
        printf("lstat error\n");
        exit(0);
    }

    ss->content_length = st.st_size;
    sprintf(ss->slength, "Content-Length: %ld\r\n\r\n", st.st_size);
    getResponseHead(re);

    /*
     printf("%s", re->buf);*/
    send(re->fd, ss->buf, ss->length, 0);

    char *buf = (char *) malloc(sizeof(char) * ss->content_length);

    int fdo = open(re->rpath, O_RDONLY);

    if (fdo < 0) {
        printf("open error\n");
        /*todo  open errro send other info*/
        exit(0);
    }

    if (read(fdo, buf, ss->content_length) < 0) {

        printf("read error\n");
        exit(0);
    }
    send(re->fd, buf, ss->content_length, 0);
    close(fdo);
    free(buf);

}

void sendResponse(request *re) {
    send(re->fd, re->res.buf, re->res.length, 0);

}

void getHeadResponseHead(request *re) {

    response *ss = &(re->res);
    char *index = ss->buf;

    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, "HTTP/1.0 200 OK\r\n");
            ss->length = 13 + 4;/*version state*/
            index += 17;
            break;
        case R_FORBIDDEN:
            strcpy(index, "HTTP/1.0 403 Permission Denied\r\n");
            ss->length = 13 + 19;/*version state*/
            index += 32;
            break;

        case R_NOT_FOUND:
            strcpy(index, "HTTP/1.0 404 NOT FOUND\r\n");
            ss->length = 13 + 11;/*version state*/
            index += 24;
            break;
    }
    strcpy(index, "Content-Type: text/html\r\n");
    index += 25;
    ss->length += 25;/*version state*/

    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, ss->slength);

            ss->length += strlen(ss->slength);/*version state*/
            index += strlen(ss->slength);
            break;
        case R_FORBIDDEN:
            strcpy(index, "Content-Length: 0\r\n\r\n");
            ss->length += 21;/*send:forbidden.*/
            index += 21;
            *index = '\0';
            break;

        case R_NOT_FOUND:
            strcpy(index, "Content-Length: 0\r\n\r\n");
            ss->length += 21;/*send:not found.*/
            index += 21;
            *index = '\0';
            break;
    }

}

void getResponseHead(request *re) {
    response *ss = &(re->res);
    char *index = ss->buf;

    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, "HTTP/1.0 200 OK\r\n");
            ss->length = 13 + 4;/*version state*/
            index += 17;
            break;
        case R_FORBIDDEN:
            strcpy(index, "HTTP/1.0 403 FORBIDDEN\r\n");
            ss->length = 13 + 11;/*version state*/
            index += 24;
            break;

        case R_NOT_FOUND:
            strcpy(index, "HTTP/1.0 404 NOT FOUND\r\n");
            ss->length = 13 + 11;/*version state*/
            index += 24;
            break;
    }
    strcpy(index, "Content-Type: text/html\r\n");
    index += 25;
    ss->length += 25;/*version state*/

    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, ss->slength);

            ss->length += strlen(ss->slength);/*version state*/
            index += strlen(ss->slength);
            break;
        case R_FORBIDDEN:
            strcpy(index, "Content-Length: 80\r\n\r\n");
            ss->length += 22;/*send:forbidden.*/
            index += 22;

            char
                    bb[] =
                    "<table> <head><title>not found</title></head> <body>403 Permission Denied</body></table>";
            strcpy(index, bb);
            /*printf("aa:%s %d\n", bb, strlen(bb));*/
            ss->length += strlen(bb);
            index += strlen(bb);
            *index = '\0';
            break;

        case R_NOT_FOUND:
            strcpy(index, "Content-Length: 80\r\n\r\n");
            ss->length += 22;/*send:not found.*/
            index += 22;

            char
                    aa[] =
                    "<table> <head><title>not found</title></head> <body>404 not found</body></table>";
            strcpy(index, aa);
            /*printf("aa:%s %d\n", aa, strlen(aa));*/
            ss->length += strlen(aa);
            index += strlen(aa);
            *index = '\0';
            break;
    }

}

void generateResHead(request *re) {
    response *ss = &(re->res);
    char *index = ss->buf;
    int len;
    strcpy(ss->buf, "HTTP/1.0 ");

    ss->length = 8 + 1 + 3 + 1;/*version state*/

    switch (ss->state) {
        case R_REQUEST_OK:
            len = 3;
            break;
        case R_FORBIDDEN:
            len = 10;
            break;

        case R_NOT_FOUND:
            len = 10;
            break;

    }
    ss->sstate = (char *) malloc(sizeof(char) * len);
    if (ss->sstate == NULL) {
        printf("lstat error\n");
        exit(0);
    }
    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(ss->sstate, "ok");
            break;
        case R_FORBIDDEN:
            strcpy(ss->sstate, "forbidden");
            break;

        case R_NOT_FOUND:
            strcpy(ss->sstate, "not found");
            break;

    }

    ss->length += len;
    len = 18;
    /*Server: httpd/1.0*/
    ss->server = (char *) malloc(len * sizeof(char));

    if (ss->server == NULL) {
        printf("lstat error\n");
        exit(0);
    }
    strcpy(ss->server, "Server:httpd/1.0");

    ss->cdate = (char *) malloc(100 * sizeof(char));
    strcpy(ss->cdate, "Date:Wed, 30 May 2012 13:10:08 GMT");

    ss->length += 35;
    /*
     ss->buf = (char *) malloc(ss->length * sizeof(char));
     */
    if (ss->buf == NULL) {
        printf("lstat error\n");
        exit(0);
    }

    index = ss->buf;

    strcpy(index, ss->version);
    index += 8;

    *index = ' ';
    ++index;
    switch (ss->state) {
        case R_REQUEST_OK:
            strcpy(index, "200");

            break;
        case R_FORBIDDEN:
            strcpy(index, "403");
            break;

        case R_NOT_FOUND:
            strcpy(index, "404");
            break;
    }
    index += 3;
    *index = ' ';
    ++index;

    strcpy(index, ss->sstate);
    index += strlen(ss->sstate);
    *index = '\n';
    ++index;

    strcpy(index, ss->server);
    index += strlen(ss->server);
    *index = '\n';
    ++index;
    *index = '\n';
    ++index;

    /*send the head*/
    send(re->fd, re->res.buf, re->res.length, 0);
    free(ss->buf);
    /*ss->buf = (char *) malloc(ss->content_length * sizeof(char));*/
    index = ss->buf;
    int fdo = open(re->rpath, O_RDONLY);

    if (fdo < 0) {
        printf("open error\n");
        /*todo  open errro send other info*/
        exit(0);
    }

    if (read(fdo, index, ss->content_length) < 0) {

        printf("read error\n");
        exit(0);
    }
    send(re->fd, index, ss->content_length, 0);

}

void checkFilePath(request *re) {

    char *path = re->file;


    if (*path == '/')
        ++path;

    if (*path == '.' && *(path + 1) == '.') {
        /*go to the parent dir */
        re->solution = SOLU_BAD_REQUEST;
        re->res.state = R_BAD_REQUEST;
        return;
    } else {

        checkCGI(re);

        if (re->is_cgi == CGI) {
            re->solution = SOLU_CGI;
        } else {
            /*get other behavior*/
            re->solution = SOLU_OTHER;
        }

    }
    /*else if(*path='/'){


     }
     */

}

void checkCGI(request *re) {

    char *p = re->file;

    char *pp = strstr(re->file, "cgi-like");

    if (pp == NULL) {
        re->is_cgi = NPH;
    } else {
        /*todo
         * check the cgi path right*/
        re->is_cgi = CGI;
    }

}
