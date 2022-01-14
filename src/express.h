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

typedef struct request_t
{
  char *path;
  char *method;
  char *url;
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
  void (^render)(char *, ...);
  void *headersHash;
  void (^set)(char *, char *);
  void *cookiesHash;
  void (^cookie)(char *, char *);
  void (^json)(char *, ...);
  void (^location)(char *);
  void (^redirect)(char *);
  int status;
} response_t;

typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^middlewareHandler)(request_t *req, response_t *res, void (^next)());

typedef struct app_t
{
  void (^get)(char *path, requestHandler);
  void (^post)(char *path, requestHandler);
  void (^put)(char *path, requestHandler);
  void (^delete)(char *path, requestHandler);
  void (^listen)(int port, void (^handler)());
  void (^use)(middlewareHandler);
} app_t;

app_t express();

middlewareHandler expressStatic(char *path);
middlewareHandler memSessionMiddlewareFactory();

#endif // EXPRESS_H
