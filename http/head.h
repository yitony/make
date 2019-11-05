#define SOCKETBUF_SIZE                8192
#define MAX_HEADER_LENGTH            1024
#define CLIENT_STREAM_SIZE            SOCKETBUF_SIZE
#define BUFFER_SIZE                    CLIENT_STREAM_SIZE
#define SLENGTH                    1024
#define REQUEST_TIMEOUT                60

#define CGI_MIME_TYPE               "application/x-httpd-cgi"

#define MAX_SITENAME_LENGTH            256
#define MAX_LOG_LENGTH                MAX_HEADER_LENGTH + 1024
#define MAX_FILE_LENGTH                NAME_MAX
#define MAX_PATH_LENGTH                PATH_MAX

#ifndef SERVER_VERSION
#define SERVER_VERSION                "httpd/2.0.01"
#endif

#define CGI_VERSION                    "CGI/1.1"
#define COMMON_CGI_COUNT            6
#define CGI_ENV_MAX                    50
#define CGI_ARGC_MAX                128

/******************* RESPONSE CLASSES *****************/

#define R_INFORMATIONAL    1
#define R_SUCCESS    2
#define R_REDIRECTION    3
#define R_CLIENT_ERROR    4
#define R_SERVER_ERROR    5

/******************* RESPONSE CODES ******************/

#define R_REQUEST_OK    200   /*ok*/
#define R_CREATED        201
#define R_ACCEPTED        202
#define R_PROVISIONAL    203       /* provisional information */
#define R_NO_CONTENT    204

#define R_MULTIPLE        300          /* multiple choices */
#define R_MOVED_PERM    301
#define R_MOVED_TEMP    302
#define R_NOT_MODIFIED    304

#define R_BAD_REQUEST    400
#define R_UNAUTHORIZED    401
#define R_PAYMENT        402           /* payment required */
#define R_FORBIDDEN        403     /*forbidden*/
#define R_NOT_FOUND        404    /* not found */
#define R_METHOD_NA        405         /* method not allowed */
#define R_NONE_ACC        406          /* none acceptable */
#define R_PROXY            407            /* proxy authentication required */
#define R_REQUEST_TO    408        /* request timeout */
#define R_CONFLICT        409
#define R_GONE            410

#define R_ERROR            500            /* internal server error */
#define    R_NOT_IMP        501           /* not implemented */
#define    R_BAD_GATEWAY    502
#define R_SERVICE_UNAV    503      /* service unavailable */
#define    R_GATEWAY_TO    504        /* gateway timeout */
#define R_BAD_VERSION    505

/****************** METHODS *****************/

#define M_GET        1
#define M_HEAD        2
#define M_PUT        3
#define M_POST        4
#define M_DELETE    5
#define M_UNKNOWN    6

/************** REQUEST STATUS (req->status) ***************/

#define READ_HEADER             0
#define ONE_CR                  1
#define ONE_LF                  2
#define TWO_CR                  3
#define BODY_READ               4
#define BODY_WRITE              5
#define WRITE                   6
#define PIPE_READ               7
#define PIPE_WRITE              8
#define DONE                    9
#define DEAD                   10

/************** CGI TYPE (req->is_cgi) ******************/

#define CGI                     1
#define NPH                     2

/**************solution******************/
#define  SOLU_BAD_REQUEST    1
#define  SOLU_CGI        2
#define  SOLU_OTHER        3

typedef struct parm {

    long port;

} parm;

typedef struct response {

    char version[9]; /*version info*/
    char *sstate; /* */
    char *server; /*service info*/
    char *cdate; /*cur time*/
    char *conten_type; /*Content-Type*/

    int state;
    long content_length; /*Content-Length*/
    char slength[SLENGTH];
    int length;
    char buf[BUFFER_SIZE];

} response;

typedef struct cgi {

    char *path;
/*
 int num;
 char **parms;
 */
} cgi;

typedef struct request {
    int fd; /* client's socket fd */
    int status; /* see #defines.h */
    int efd;

    char *file; /* pathname of requested file */
    char *rpath;

    unsigned long filesize; /* filesize */
    unsigned long filepos; /* position in file */

    int method; /* M_GET, M_POST, etc. */

    response res;

    int solution;

    int is_cgi;

    char *buf;
    int reclen;

    cgi c;

} request;

typedef struct threadArg {
    int fd;
    int efd;
    pthread_mutex_t *lock;
    struct threadArg *next;
    struct threadArg *pre;

} threadArg;

#define tnode threadArg

typedef struct service {

    long port;
    int lsocket;
    tnode *tlist;

} service;

void service_init(int argc, char *args[], service *se);

int createService(service *se);

void service_destroy(service *se);

void printUsage();

int handleRequest(service *se);

void getRequest(request *re, int cs);

void initRequest(request *re);

void destroyRequest(request *re);

void getHead(request *re, char *buf, int len);

void getMethod(request *re, char *from, int len);

void toLow(char *buf);

void getFilePath(request *re, char *from, int len);

void printRequest(request *re);

void initResponse(response *res);

void destroyResponse(response *res);

void initResponse(response *res);

void destroyResponse(response *res);

void getResponse(request *re);

void checkFilePath(request *re);

void checkCGI(request *re);

void generateResHead(request *re);

void sendResponse(request *re);

void gethandler(request *re);

void headHandler(request *re);

void forbidResponse(request *re);

void notfoundResponse(request *re);

void okResponse(request *re);

void cgiHandler(request *re);

void getResponseHead(request *re);

void notfoundHeadResponse(request *re);

void forbidHeadResponse(request *re);

void okHeadResponse(request *re);

void getHeadResponseHead(request *re);

void badRequestHandle(request *re);

void checkCGIPath(request *re);
