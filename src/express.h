#ifndef EXPRESS_H
#define EXPRESS_H

#define UNUSED __attribute__((unused))

typedef struct session_t
{
  char *uuid;
  void *store;
  void * (^get)(char *key);
  void (^set)(char *key, void *value);
} session_t;

typedef struct cookie_opts_t
{
  char *name;
  char *value;
  char *domain;
  char *path;
  int maxAge;
  int secure;
  int httpOnly;
} cookie_opts_t;

typedef struct request_t
{
  char *path;
  char *method;
  char *url;         // TODO: remove
  char *baseUrl;     // TODO
  char *originalUrl; // TODO
  char *hostname;    // TODO
  char *ip;          // TODO
  char **ips;        // TODO
  char *protocol;    // TODO
  int secure;        // TODO
  void *route;       // TODO
  int xhr;           // TODO
  char **subdomains; // TODO
  char *queryString;
  void *queryHash;
  char * (^query)(char *queryKey);
  void *headersHash;
  char * (^get)(char *headerKey);
  void *paramsHash;
  char **paramValues;
  char * (^params)(char *paramKey);
  char *bodyString;
  void *bodyHash;
  char * (^body)(char *bodyKey);
  void *middlewareHash;
  void * (^m)(char *middlewareKey);
  void (^mSet)(char *middlewareKey, void *middleware);
  char *rawRequest;
  session_t *session;
  char *cookies[4096];
  void *cookiesHash;
  char * (^cookie)(char *key);
} request_t;

typedef struct response_t
{
  void (^send)(const char *, ...);
  void (^sendFile)(char *);
  void (^render)(char *, ...); // TODO
  void *headersHash;
  void (^set)(char *, char *);
  void *cookiesHash;
  void (^cookie)(char *, char *);               // TODO add cookie_opts_t
  void (^clearCookie)(char *, cookie_opts_t *); // TODO
  void (^json)(char *, ...);                    // TODO
  void (^location)(char *);
  void (^redirect)(char *);
  void (^sendStatus)(int);          // TODO
  void (^download)(char *, char *); // TODO
  int status;
} response_t;

typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^middlewareHandler)(request_t *req, response_t *res, void (^next)());
typedef void (^errorHandler)(request_t *req, response_t *res, int err);                          // TODO
typedef void (^paramHandler)(request_t *req, response_t *res, void (^next)(), char *paramValue); // TODO

typedef struct router_t // TODO
{
  void (^get)(char *path, requestHandler);
  void (^post)(char *path, requestHandler);
  void (^put)(char *path, requestHandler);
  void (^delete)(char *path, requestHandler);
  void (^all)(char *path, requestHandler);
  void (^use)(middlewareHandler);
  void (^useRouter)(char *path, struct router_t *router);
  void (^param)(char *param, paramHandler);
} router_t;

typedef struct app_t
{
  void (^get)(char *path, requestHandler);
  void (^post)(char *path, requestHandler);
  void (^put)(char *path, requestHandler);
  void (^delete)(char *path, requestHandler);
  void (^all)(char *path, requestHandler); // TODO
  void (^listen)(int port, void (^handler)());
  void (^use)(middlewareHandler);
  void (^useRouter)(char *path, router_t *router); // TODO
  void (^engine)(char *ext, void *engine);
} app_t;

app_t express();
router_t expressRouter(); // TODO

middlewareHandler expressStatic(char *path);
middlewareHandler memSessionMiddlewareFactory();

#endif // EXPRESS_H
