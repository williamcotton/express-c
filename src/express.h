#include <hash/hash.h>

#define UNUSED __attribute__((unused))

typedef struct session_t
{
  char *uuid;
  char * (^get)(char *key);
  void (^set)(char *key, char *value);
} session_t;

typedef struct cookie_t
{
  char * (^get)(char *key);
  void (^set)(char *key, char *value);
} cookie_t;

typedef struct request_t
{
  char *path;
  char *method;
  char *url;
  char *queryString;
  hash_t *queryHash;
  char * (^query)(char *queryKey);
  hash_t *headersHash;
  char * (^get)(char *headerKey);
  hash_t *paramsHash;
  char **paramValues;
  char * (^param)(char *paramKey);
  char *bodyString;
  hash_t *bodyHash;
  char * (^body)(char *bodyKey);
  hash_t *middlewareHash;
  void * (^m)(char *middlewareKey);
  void (^mSet)(char *middlewareKey, void *middleware);
  char *rawRequest;
  session_t *session;
  cookie_t *cookie;
} request_t;

typedef struct response_t
{
  void (^send)(char *);
  void (^sendf)(char *, ...);
  void (^sendFile)(char *);
  void (^render)(char *, ...);
  int status;
} response_t;

typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^middlewareHandler)(request_t *req, response_t *res, void (^next)());

typedef struct app_t
{
  void (^get)(char *path, requestHandler);
  void (^post)(char *path, requestHandler);
  void (^listen)(int port, void (^handler)());
  void (^use)(middlewareHandler);
} app_t;

app_t express();

middlewareHandler expressStatic(char *path);