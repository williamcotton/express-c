#include <picohttpparser/picohttpparser.h>
#include <curl/curl.h>

#ifndef EXPRESS_H
#define EXPRESS_H

#define UNUSED __attribute__((unused))

char *generateUuid();

typedef struct session_t
{
  char *uuid;
  void *store;
  void * (^get)(char *key);
  void (^set)(char *key, void *value);
} session_t;

typedef struct cookie_opts_t
{
  char *domain;
  char *path;
  char *expires;
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

typedef struct request_t
{
  char *path;
  char *method;
  char *_method;
  char *url;         // TODO: replace req.url with req.baseUrl and req.originalUrl
  char *baseUrl;     // TODO: add req.baseUrl
  char *originalUrl; // TODO: add req.originalUrl
  char *hostname;    // TODO: add req.hostname
  char *ip;          // TODO: add req.ip
  char **ips;        // TODO: add req.ips
  char *protocol;    // TODO: add req.protocol
  int secure;        // TODO: add req.secure
  void *route;       // TODO: add req.route
  int xhr;           // TODO: add req.xhr
  char **subdomains; // TODO: add req.subdomains
  key_value_t queryKeyValues[100];
  size_t queryKeyValueCount;
  char *queryString;
  char * (^query)(char *queryKey);
  struct phr_header headers[100];
  size_t numHeaders;
  char * (^get)(char *headerKey);
  param_match_t *paramMatch;
  char *pathMatch;
  key_value_t paramKeyValues[100];
  size_t paramKeyValueCount;
  char * (^params)(char *paramKey);
  key_value_t bodyKeyValues[100];
  size_t bodyKeyValueCount;
  const char *bodyString;
  char * (^body)(char *bodyKey);
  int middlewareStackIndex;
  void *middlewareHash;
  void * (^m)(char *middlewareKey);
  void (^mSet)(char *middlewareKey, void *middleware);
  char *rawRequest;
  char *rawRequestBody;
  session_t *session;
  char *cookiesString;
  char *cookies[4096];
  void *cookiesHash;
  char * (^cookie)(char *key);
  int mallocCount;
  req_malloc_t mallocs[1024];
  void * (^malloc)(size_t size);
  CURL *curl;
} request_t;

typedef struct response_t
{
  void (^send)(char *);
  void (^sendf)(char *, ...);
  void (^sendFile)(char *);
  void (^render)(void *, void *);
  void *headersHash;
  void (^set)(char *, char *);
  char * (^get)(char *);
  void *cookiesHash;
  int cookieHeadersLength;
  char cookieHeaders[4096];
  void (^cookie)(char *, char *, cookie_opts_t);
  void (^clearCookie)(char *, cookie_opts_t); // TODO: add res.clearCookie
  void (^json)(char *, ...);                  // TODO: add res.json
  void (^location)(char *);
  void (^redirect)(char *);
  void (^sendStatus)(int);          // TODO: add res.sendStatus
  void (^download)(char *, char *); // TODO: add res.download
  int status;
} response_t;

typedef struct error_t
{
  int status;
  char *message;
} error_t;

typedef void (^cleanupHandler)(request_t *finishedReq);
typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^middlewareHandler)(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler));
typedef void (^errorHandler)(error_t err, request_t *req, response_t *res, void (^next)());      // TODO: add errorHandler
typedef void (^paramHandler)(request_t *req, response_t *res, void (^next)(), char *paramValue); // TODO: add paramHandler

typedef struct router_t // TODO: implement router_t
{
  void (^get)(char *path, requestHandler);
  void (^post)(char *path, requestHandler);
  void (^put)(char *path, requestHandler);
  void (^patch)(char *path, requestHandler);
  void (^delete)(char *path, requestHandler);
  void (^all)(char *path, requestHandler);
  void (^use)(middlewareHandler);
  void (^useRouter)(char *path, struct router_t *router);
  void (^param)(char *param, paramHandler);
} router_t;

typedef struct server_t
{
  void (^close)();
} server_t;

typedef struct app_t
{
  void (^get)(char *path, requestHandler);
  void (^post)(char *path, requestHandler);
  void (^put)(char *path, requestHandler);
  void (^patch)(char *path, requestHandler);
  void (^delete)(char *path, requestHandler);
  void (^all)(char *path, requestHandler); // TODO: add app.all
  void (^listen)(int port, void (^handler)());
  void (^use)(middlewareHandler);
  void (^useRouter)(char *path, router_t *router); // TODO: add app.useRouter
  void (^engine)(char *ext, void *engine);
  void (^error)(errorHandler); // TODO: add app.error
} app_t;

app_t express();
router_t expressRouter(); // TODO: implement expressRouter

middlewareHandler expressStatic(char *path);
middlewareHandler memSessionMiddlewareFactory();

#endif // EXPRESS_H
