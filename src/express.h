/*
  Copyright (c) 2022 William Cotton

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef EXPRESS_H
#define EXPRESS_H

#include <Block.h>
#include <MegaMimes/MegaMimes.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <curl/curl.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <execinfo.h>
#include <picohttpparser/picohttpparser.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string/string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

#ifdef __linux__
#include <bsd/string.h>
#include <pthread.h>
#include <sys/epoll.h>
#endif

#define MAX_REQUEST_SIZE 4096
#define READ_TIMEOUT_SECS 30
#define ACCEPT_TIMEOUT_SECS 30
#define BT_BUF_SIZE 100

/* Helpers */

#define UNUSED __attribute__((unused))

#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...)                                                          \
  fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...)                                                        \
  fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__,    \
          clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...)                                                       \
  fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__,     \
          clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...)                                                       \
  fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...)                                                       \
  if (!(A)) {                                                                  \
    log_err(M, ##__VA_ARGS__);                                                 \
    errno = 0;                                                                 \
    goto error;                                                                \
  }

#define check_silent(A, M, ...)                                                \
  if (!(A)) {                                                                  \
    errno = 0;                                                                 \
    goto error;                                                                \
  }

#define sentinel(M, ...)                                                       \
  {                                                                            \
    log_err(M, ##__VA_ARGS__);                                                 \
    errno = 0;                                                                 \
    goto error;                                                                \
  }

#define check_mem(A) check((A), "Out of memory.")

#define check_debug(A, M, ...)                                                 \
  if (!(A)) {                                                                  \
    debug(M, ##__VA_ARGS__);                                                   \
    errno = 0;                                                                 \
    goto error;                                                                \
  }

#define max(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

#define min(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })

typedef void (^null)();

extern char *errorHTML;

/* Primitives */

typedef void (^freeHandler)();

typedef struct embedded_files_data_t {
  unsigned char **data;
  int *lengths;
  char **names;
  int count;
} embedded_files_data_t;

typedef struct session_t {
  const char *uuid;
  void *store;
  void * (^get)(const char *key);
  void (^set)(const char *key, void *value);
} session_t;

typedef struct cookie_opts_t {
  const char *domain;
  const char *path;
  const char *expires;
  int maxAge;
  int secure;
  int httpOnly;
} cookie_opts_t;

typedef struct param_match_t {
  char *regexRoute;
  char **keys;
  int count;
} param_match_t;

typedef struct key_value_t {
  const char *key;
  size_t keyLen;
  const char *value;
  size_t valueLen;
} key_value_t;

typedef struct key_store_t {
  const char *key;
  void *value;
} key_store_t;

typedef struct req_malloc_t {
  void *ptr;
} req_malloc_t;

typedef struct req_block_copy_t {
  void *ptr;
} req_block_copy_t;

typedef struct error_t {
  int status;
  char *message;
} error_t;

/* Request */

typedef struct request_t {
  const char *path;
  const char *method;
  const char *_method;
  char *url;
  const char *baseUrl;
  const char *originalUrl;
  const char *hostname;
  const char *ip;
  int ipsCount;
  const char **ips;
  const char *protocol;
  int secure;
  void *route;
  int xhr;
  int subdomainsCount;
  const char **subdomains;
  char rawRequest[MAX_REQUEST_SIZE];
  size_t rawRequestSize;
  key_value_t queryKeyValues[100];
  size_t queryKeyValueCount;
  const char *queryString;
  char * (^query)(const char *queryKey);
  struct phr_header headers[100];
  size_t numHeaders;
  char * (^get)(const char *headerKey);
  const char *pathMatch;
  key_value_t paramKeyValues[100];
  size_t paramKeyValueCount;
  char * (^params)(const char *paramKey);
  key_value_t bodyKeyValues[100];
  size_t bodyKeyValueCount;
  char *bodyString;
  char * (^body)(const char *bodyKey);
  int middlewareStackCount;
  key_store_t middlewareKeyValues[100];
  size_t middlewareKeyValueCount;
  void * (^m)(const char *middlewareKey);
  void (^mSet)(const char *middlewareKey, void *middleware);
  long long contentLength;
  session_t *session;
  const char *cookiesString;
  const char *cookies[4096];
  key_value_t cookiesKeyValues[100];
  size_t cookiesKeyValueCount;
  char * (^cookie)(const char *key);
  int mallocCount;
  req_malloc_t mallocs[1024];
  void * (^malloc)(size_t size);
  int blockCopyCount;
  req_block_copy_t blockCopies[1024];
  void * (^blockCopy)(void *);
  CURL *curl;
  void **middlewareCleanupBlocks;
  char *XRequestedWith;
  char *XForwardedFor;
  int trashableCount;
  freeHandler trashables[1024];
  void (^trash)(freeHandler);
} request_t;

/* Response */

typedef struct response_t {
  void (^send)(const char *);
  void (^sendf)(const char *, ...);
  void (^sendFile)(const char *);
  void (^render)(void *, void *);
  error_t *err;
  void (^error)(error_t *err);
  void *headersHash;
  void (^set)(const char *, const char *);
  char * (^get)(const char *);
  key_value_t headersKeyValues[100];
  size_t headersKeyValueCount;
  size_t cookieHeadersLength;
  char cookieHeaders[4096];
  void (^cookie)(const char *, const char *, cookie_opts_t);
  void (^clearCookie)(const char *, cookie_opts_t);
  void (^json)(const char *);
  void (^location)(const char *);
  void (^redirect)(const char *);
  void (^sendStatus)(int);
  void (^type)(const char *);
  void (^download)(const char *, const char *);
  int status;
  int didSend;
} response_t;

/* Handlers */

typedef void (^cleanupHandler)(request_t *finishedReq);
typedef void (^appCleanupHandler)();
typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^middlewareHandler)(request_t *req, response_t *res,
                                  void (^next)(),
                                  void (^cleanup)(cleanupHandler));
typedef void (^errorHandler)(error_t *err, request_t *req, response_t *res,
                             void (^next)());
typedef void (^paramHandler)(request_t *req, response_t *res,
                             const char *paramValue, void (^next)(),
                             void (^cleanup)(cleanupHandler));

/* Function signatures */

typedef char * (^getBlock)(const char *key);
typedef void * (^mallocBlock)(size_t);
typedef void * (^copyBlock)(void *);
typedef void (^trashBlock)(freeHandler);
typedef void (^getMiddlewareSetBlock)(const char *key, void *middleware);
typedef void * (^getMiddlewareBlock)(const char *key);
typedef void (^sendBlock)(const char *body);
typedef void (^sendfBlock)(const char *format, ...);
typedef void (^sendStatusBlock)(int status);
typedef void (^downloadBlock)(const char *filePath, const char *name);
typedef void (^setBlock)(const char *key, const char *value);
typedef void (^setError)(error_t *err);
typedef void (^setCookie)(const char *cookieKey, const char *cookieValue,
                          cookie_opts_t opts);
typedef void (^clearCookieBlock)(const char *key, cookie_opts_t opts);

/* Public functions */

char *generateUuid();
int writePid(char *pidFile);
unsigned long readPid(char *pidFile);
char *cwdFullPath(const char *path);
char *matchEmbeddedFile(const char *path, embedded_files_data_t embeddedFiles);

/* Public middleware */

typedef struct mem_session_store_t {
  key_value_t keyValues[100];
  int count;
} mem_session_store_t;

typedef struct mem_store_t {
  char *uuid;
  mem_session_store_t *sessionStore;
} mem_store_t;

typedef struct mem_session_t {
  mem_store_t **stores;
  int count;
} mem_session_t;

middlewareHandler expressStatic(const char *path, const char *fullPath,
                                embedded_files_data_t embeddedFiles);
middlewareHandler memSessionMiddlewareFactory(mem_session_t *memSession,
                                              dispatch_queue_t memSessionQueue);

/* expressRouter */

typedef struct route_handler_t {
  char *basePath;
  const char *method;
  const char *path;
  int regex;
  param_match_t *paramMatch;
  requestHandler handler;
} route_handler_t;

typedef struct middleware_t {
  middlewareHandler handler;
} middleware_t;

typedef struct param_handler_t {
  const char *paramKey;
  paramHandler handler;
} param_handler_t;

typedef struct router_t {
  const char *basePath;
  const char *mountPath;
  int isBaseRouter;
  void (^get)(const char *path, requestHandler);
  void (^post)(const char *path, requestHandler);
  void (^put)(const char *path, requestHandler);
  void (^patch)(const char *path, requestHandler);
  void (^delete)(const char *path, requestHandler);
  void (^all)(const char *path, requestHandler);
  void (^use)(middlewareHandler);
  void (^useRouter)(char *mountPath, struct router_t *routerToMount);
  void (^mountTo)(struct router_t *baseRouter);
  void (^param)(const char *paramKey, paramHandler);
  void (^error)(errorHandler);
  void (^handler)(request_t *req, response_t *res);
  void (^cleanup)(appCleanupHandler);
  void (^free)();
  route_handler_t *routeHandlers;
  int routeHandlerCount;
  middleware_t *middlewares;
  int middlewareCount;
  struct router_t **routers;
  int routerCount;
  appCleanupHandler *appCleanupBlocks;
  int appCleanupCount;
  param_handler_t *paramHandlers;
  int paramHandlerCount;
  errorHandler *errorHandlers;
  int errorHandlerCount;
} router_t;

router_t *expressRouter();

/* server */

typedef struct server_t {
  int socket;
  int port;
  dispatch_queue_t serverQueue;
  void (^close)();
  int (^listen)(int port);
  int (^initSocket)();
  void (^free)();
} server_t;

server_t *expressServer();

/* client */

typedef struct client_t {
  int socket;
  char *ip;
} client_t;

/* express */

typedef struct app_t {
  server_t *server;
  void (^get)(const char *path, requestHandler);
  void (^post)(const char *path, requestHandler);
  void (^put)(const char *path, requestHandler);
  void (^patch)(const char *path, requestHandler);
  void (^delete)(const char *path, requestHandler);
  void (^all)(const char *path, requestHandler);
  void (^listen)(int port, void (^callback)());
  void (^use)(middlewareHandler);
  void (^useRouter)(char *mountPath, struct router_t *routerToMount);
  void (^param)(const char *paramKey, paramHandler);
  void (^engine)(const char *ext, const void *engine);
  void (^error)(errorHandler);
  void (^cleanup)(appCleanupHandler);
  void (^closeServer)();
  void (^free)();
} app_t;

void shutdownAndFreeApp(app_t *app);

app_t *express();

#endif // EXPRESS_H
