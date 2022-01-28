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

#include <picohttpparser/picohttpparser.h>
#include <dispatch/dispatch.h>
#include <hash/hash.h>
#include <curl/curl.h>

#ifndef EXPRESS_H
#define EXPRESS_H

typedef struct embedded_files_data_t
{
  unsigned char **data;
  int *lengths;
  char **names;
  int count;
} embedded_files_data_t;

#define UNUSED __attribute__((unused))

char *generateUuid();
int writePid(char *pidFile);

typedef struct session_t
{
  const char *uuid;
  void *store;
  void * (^get)(const char *key);
  void (^set)(const char *key, void *value);
} session_t;

typedef struct cookie_opts_t
{
  const char *domain;
  const char *path;
  const char *expires;
  int maxAge;
  int secure;
  int httpOnly;
} cookie_opts_t;

typedef struct param_match_t
{
  char *regexRoute;
  char **keys;
  int count;
} param_match_t;

typedef struct key_value_t
{
  const char *key;
  size_t keyLen;
  const char *value;
  size_t valueLen;
} key_value_t;

typedef struct req_malloc_t
{
  void *ptr;
} req_malloc_t;

typedef struct req_block_copy_t
{
  void *ptr;
} req_block_copy_t;

typedef struct request_t
{
  const char *path;
  const char *method;
  const char *_method;
  const char *url;         // TODO: replace req.url with req.baseUrl and req.originalUrl
  const char *baseUrl;     // TODO: add req.baseUrl
  const char *originalUrl; // TODO: add req.originalUrl
  const char *hostname;    // TODO: add req.hostname
  const char *ip;          // TODO: add req.ip
  const char **ips;        // TODO: add req.ips
  const char *protocol;    // TODO: add req.protocol
  int secure;              // TODO: add req.secure
  void *route;             // TODO: add req.route
  int xhr;                 // TODO: add req.xhr
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
  const char *bodyString;
  char * (^body)(const char *bodyKey);
  int middlewareStackIndex;
  void *middlewareHash;
  void * (^m)(const char *middlewareKey);
  void (^mSet)(const char *middlewareKey, void *middleware);
  const char *rawRequest;
  const char *rawRequestBody;
  session_t *session;
  const char *cookiesString;
  const char *cookies[4096];
  void *cookiesHash;
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

typedef struct response_t
{
  void (^send)(const char *);
  void (^sendf)(const char *, ...);
  void (^sendFile)(const char *);
  void (^render)(void *, void *);
  void *headersHash;
  void (^set)(const char *, const char *);
  char * (^get)(const char *);
  void *cookiesHash;
  int cookieHeadersLength;
  char cookieHeaders[4096];
  void (^cookie)(const char *, const char *, cookie_opts_t);
  void (^clearCookie)(const char *, cookie_opts_t); // TODO: add res.clearCookie
  void (^json)(const char *, ...);                  // TODO: add res.json
  void (^location)(const char *);
  void (^redirect)(const char *);
  void (^sendStatus)(int);                      // TODO: add res.sendStatus
  void (^download)(const char *, const char *); // TODO: add res.download
  int status;
} response_t;

typedef struct error_t
{
  int status;
  char *message;
} error_t;

typedef void (^cleanupHandler)(request_t *finishedReq);
typedef void (^appCleanupHandler)();
typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^middlewareHandler)(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler));
typedef void (^errorHandler)(error_t err, request_t *req, response_t *res, void (^next)());            // TODO: add errorHandler
typedef void (^paramHandler)(request_t *req, response_t *res, void (^next)(), const char *paramValue); // TODO: add paramHandler

typedef struct route_handler_t
{
  char *basePath;
  const char *method;
  const char *path;
  int regex;
  param_match_t *paramMatch;
  requestHandler handler;
} route_handler_t;

typedef struct middleware_t
{
  const char *path;
  middlewareHandler handler;
} middleware_t;

typedef struct router_t // TODO: implement router_t
{
  void (^get)(const char *path, requestHandler);
  void (^post)(const char *path, requestHandler);
  void (^put)(const char *path, requestHandler);
  void (^patch)(const char *path, requestHandler);
  void (^delete)(const char *path, requestHandler);
  void (^all)(const char *path, requestHandler);
  void (^use)(middlewareHandler);
  void (^useRouter)(const char *path, struct router_t *router);
  void (^param)(const char *param, paramHandler);
  route_handler_t *routeHandlers;
  int routeHandlerCount;
  middleware_t *middlewares;
  int middlewareCount;
} router_t;

typedef struct server_t
{
  void (^close)();
} server_t;

typedef struct app_t
{
  void (^get)(const char *path, requestHandler);
  void (^post)(const char *path, requestHandler);
  void (^put)(const char *path, requestHandler);
  void (^patch)(const char *path, requestHandler);
  void (^delete)(const char *path, requestHandler);
  void (^all)(const char *path, requestHandler); // TODO: add app.all
  void (^listen)(int port, void (^handler)());
  void (^use)(middlewareHandler);
  void (^useRouter)(const char *path, router_t *router); // TODO: add app.useRouter
  void (^engine)(const char *ext, const void *engine);
  void (^error)(errorHandler); // TODO: add app.error
  void (^cleanup)(appCleanupHandler);
  void (^closeServer)(int status);
} app_t;

app_t express();
router_t expressRouter(); // TODO: implement expressRouter

char *cwdFullPath(const char *path);
char *matchEmbeddedFile(const char *path, embedded_files_data_t embeddedFiles);

middlewareHandler expressStatic(const char *path, const char *fullPath, embedded_files_data_t embeddedFiles);
middlewareHandler memSessionMiddlewareFactory(hash_t *memSessionStore, dispatch_queue_t memSessionQueue);

#endif // EXPRESS_H
