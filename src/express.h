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

#include <curl/curl.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <picohttpparser/picohttpparser.h>
#include <stdio.h>
#include <string.h>

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

/* Primitives */

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
  const char **ips; // TODO: add req.ips
  const char *protocol;
  int secure;
  void *route;
  int xhr;
  const char **subdomains; // TODO: add req.subdomains
  key_value_t queryKeyValues[100];
  size_t queryKeyValueCount;
  const char *queryString;
  char * (^query)(const char *queryKey);
  struct phr_header headers[100];
  size_t numHeaders;
  char * (^get)(const char *headerKey);
  param_match_t *paramMatch;
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
  const char *rawRequest;
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
} request_t;

/* Response */

typedef struct response_t {
  void (^send)(const char *);
  void (^sendf)(const char *, ...);
  void (^sendFile)(const char *);
  void (^render)(void *, void *);
  void *headersHash;
  void (^set)(const char *, const char *);
  char * (^get)(const char *);
  key_value_t headersKeyValues[100];
  size_t headersKeyValueCount;
  size_t cookieHeadersLength;
  char cookieHeaders[4096];
  void (^cookie)(const char *, const char *, cookie_opts_t);
  void (^clearCookie)(const char *, cookie_opts_t); // TODO: add res.clearCookie
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
typedef void (^errorHandler)(error_t err, request_t *req, response_t *res,
                             void (^next)());
typedef void (^paramHandler)(request_t *req, response_t *res, void (^next)(),
                             const char *paramValue);

/* Public functions */

char *generateUuid();
int writePid(char *pidFile);
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
  const char *path;
  middlewareHandler handler;
} middleware_t;

typedef struct router_t {
  const char *basePath;
  void (^get)(const char *path, requestHandler);
  void (^post)(const char *path, requestHandler);
  void (^put)(const char *path, requestHandler);
  void (^patch)(const char *path, requestHandler);
  void (^delete)(const char *path, requestHandler);
  void (^all)(const char *path, requestHandler); // TODO: add router.all
  void (^use)(middlewareHandler);
  void (^useRouter)(char *basePath, struct router_t *router);
  void (^param)(const char *param, paramHandler); // TODO: add router.param
  void (^handler)(request_t *req, response_t *res);
  route_handler_t *routeHandlers;
  int routeHandlerCount;
  middleware_t *middlewares;
  int middlewareCount;
  struct router_t **routers;
  int routerCount;
} router_t;

router_t *expressRouter(int basePath);

/* server */

typedef struct server_t {
  int socket;
  int port;
  dispatch_queue_t serverQueue;
  void (^close)();
} server_t;

/* express */

typedef struct app_t {
  void (^get)(const char *path, requestHandler);
  void (^post)(const char *path, requestHandler);
  void (^put)(const char *path, requestHandler);
  void (^patch)(const char *path, requestHandler);
  void (^delete)(const char *path, requestHandler);
  void (^all)(const char *path, requestHandler); // TODO: add app.all
  void (^listen)(int port, void (^callback)());
  void (^use)(middlewareHandler);
  void (^useRouter)(char *basePath, router_t *router);
  void (^engine)(const char *ext, const void *engine);
  void (^error)(errorHandler); // TODO: add app.error
  void (^cleanup)(appCleanupHandler);
  void (^closeServer)();
} app_t;

app_t express();

#endif // EXPRESS_H
