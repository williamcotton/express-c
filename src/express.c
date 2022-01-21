#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <dispatch/dispatch.h>
#include <regex.h>
#include <Block.h>
#include <picohttpparser/picohttpparser.h>
#include <sys/stat.h>
#include <hash/hash.h>
#include <signal.h>
#include <curl/curl.h>
#include <uuid/uuid.h>
#include <sys/errno.h>
#include "express.h"

// #define CLIENT_NET_DEBUG
// #define MIDDLEWARE_DEBUG

static char *errorHTML = "<!DOCTYPE html>\n"
                         "<html lang=\"en\">\n"
                         "<head>\n"
                         "<meta charset=\"utf-8\">\n"
                         "<title>Error</title>\n"
                         "</head>\n"
                         "<body>\n"
                         "<pre>Cannot GET %s</pre>\n"
                         "</body>\n"
                         "</html>\n";

static char *getStatusMessage(int status)
{
  switch (status)
  {
  case 100:
    return "Continue";
  case 101:
    return "Switching Protocols";
  case 102:
    return "Processing";
  case 103:
    return "Early Hints";
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 202:
    return "Accepted";
  case 203:
    return "Non-Authoritative Information";
  case 204:
    return "No Content";
  case 205:
    return "Reset Content";
  case 206:
    return "Partial Content";
  case 207:
    return "Multi-Status";
  case 208:
    return "Already Reported";
  case 226:
    return "IM Used";
  case 300:
    return "Multiple Choices";
  case 301:
    return "Moved Permanently";
  case 302:
    return "Found";
  case 303:
    return "See Other";
  case 304:
    return "Not Modified";
  case 305:
    return "Use Proxy";
  case 306:
    return "Switch Proxy";
  case 307:
    return "Temporary Redirect";
  case 308:
    return "Permanent Redirect";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 402:
    return "Payment Required";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 406:
    return "Not Acceptable";
  case 407:
    return "Proxy Authentication Required";
  case 408:
    return "Request Timeout";
  case 409:
    return "Conflict";
  case 410:
    return "Gone";
  case 411:
    return "Length Required";
  case 412:
    return "Precondition Failed";
  case 413:
    return "Payload Too Large";
  case 414:
    return "URI Too Long";
  case 415:
    return "Unsupported Media Type";
  case 416:
    return "Range Not Satisfiable";
  case 417:
    return "Expectation Failed";
  case 418:
    return "I'm a teapot";
  case 421:
    return "Misdirected Request";
  case 422:
    return "Unprocessable Entity";
  case 423:
    return "Locked";
  case 424:
    return "Failed Dependency";
  case 425:
    return "Too Early";
  case 426:
    return "Upgrade Required";
  case 428:
    return "Precondition Required";
  case 429:
    return "Too Many Requests";
  case 431:
    return "Request Header Fields Too Large";
  case 451:
    return "Unavailable For Legal Reasons";
  case 500:
    return "Internal Server Error";
  case 501:
    return "Not Implemented";
  case 502:
    return "Bad Gateway";
  case 503:
    return "Service Unavailable";
  case 504:
    return "Gateway Timeout";
  case 505:
    return "HTTP Version Not Supported";
  case 506:
    return "Variant Also Negotiates";
  case 507:
    return "Insufficient Storage";
  case 508:
    return "Loop Detected";
  case 510:
    return "Not Extended";
  case 511:
    return "Network Authentication Required";
  default:
    return "Unknown";
  }
}

size_t fileSize(char *filePath)
{
  struct stat st;
  stat(filePath, &st);
  return st.st_size;
}

static void toUpper(char *givenStr)
{
  int i;
  for (i = 0; givenStr[i] != '\0'; i++)
  {
    if (givenStr[i] >= 'a' && givenStr[i] <= 'z')
    {
      givenStr[i] = givenStr[i] - 32;
    }
  }
}

static void parseQueryStringHash(hash_t *hash, char *string)
{
  CURL *curl = curl_easy_init();
  if (curl)
  {
    char *query = strdup(string);
    char *tokens = query;
    char *p;
    while ((p = strsep(&tokens, "&\n")))
    {
      char *key = strtok(p, "=");
      char *value = NULL;
      if (key && (value = strtok(NULL, "=")))
      {
        char *decodedKey = curl_easy_unescape(curl, key, strlen(key), NULL);
        char *decodedValue = curl_easy_unescape(curl, value, strlen(value), NULL);
        hash_set(hash, decodedKey, decodedValue);
      }
      else
      {
        hash_set(hash, key, "");
      }
    }
    curl_easy_cleanup(curl);
    free(query);
    free(tokens);
    free(p);
  }
}

static void parseQueryString(const char *buf, const char *bufEnd, query_string_t *queryStrings, size_t *queryStringCount, size_t maxQueryStrings)
{
  const char *keyStart = buf;
  const char *keyEnd = NULL;
  const char *valueStart = NULL;
  const char *valueEnd = NULL;
  size_t keyLen = 0;
  size_t valueLen = 0;
  while (buf <= bufEnd)
  {
    if (*buf == '=')
    {
      keyEnd = buf;
      keyLen = keyEnd - keyStart;
      valueStart = buf + 1;
    }
    else if (*buf == '&' || *buf == '\0')
    {
      valueEnd = buf;
      valueLen = valueEnd - valueStart;
      if (*queryStringCount < maxQueryStrings)
      {
        queryStrings[*queryStringCount].key = keyStart;
        queryStrings[*queryStringCount].keyLen = keyLen;
        queryStrings[*queryStringCount].value = valueStart;
        queryStrings[*queryStringCount].valueLen = valueLen;
        (*queryStringCount)++;
      }
      keyStart = buf + 1;
    }
    buf++;
  }
}

typedef struct client_t
{
  int socket;
  char *ip;
} client_t;

param_match_t *paramMatch(char *route)
{
  param_match_t *pm = malloc(sizeof(param_match_t));
  pm->keys = malloc(sizeof(char *));
  pm->count = 0;
  char regexRoute[4096];
  regexRoute[0] = '\0';
  char *source = route;
  char *regexString = ":([A-Za-z0-9_]*)";
  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;
  char *cursor;

  if (regcomp(&regexCompiled, regexString, REG_EXTENDED))
  {
    printf("Could not compile regular expression.\n");
    free(pm);
    return NULL;
  };

  cursor = source;
  for (m = 0; m < maxMatches; m++)
  {
    if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))
      break; // No more matches

    unsigned int g = 0;
    unsigned int offset = 0;
    for (g = 0; g < maxGroups; g++)
    {
      if (groupArray[g].rm_so == (long long)(size_t)-1)
        break; // No more groups

      char cursorCopy[strlen(cursor) + 1];
      strcpy(cursorCopy, cursor);
      cursorCopy[groupArray[g].rm_eo] = 0;

      if (g == 0)
      {
        offset = groupArray[g].rm_eo;
        sprintf(regexRoute + strlen(regexRoute), "%.*s(.*)", (int)groupArray[g].rm_so, cursorCopy);
      }
      else
      {
        pm->keys = realloc(pm->keys, sizeof(char *) * (m + 1));
        pm->count++;
        char *key = malloc(sizeof(char) * (groupArray[g].rm_eo - groupArray[g].rm_so + 1));
        strncpy(key, cursorCopy + groupArray[g].rm_so, groupArray[g].rm_eo - groupArray[g].rm_so);
        key[groupArray[g].rm_eo - groupArray[g].rm_so] = '\0';
        pm->keys[m] = key;
      }
    }
    cursor += offset;
  }

  sprintf(regexRoute + strlen(regexRoute), "%s", cursor);

  regfree(&regexCompiled);

  pm->regexRoute = malloc(sizeof(char) * (strlen(regexRoute) + 1));
  strcpy(pm->regexRoute, regexRoute);

  return pm;
}

void routeMatch(request_t *req, int *match)
{
  // TODO: array of structs of *string offsets and lengths
  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;
  char *cursor;

  if (regcomp(&regexCompiled, req->paramMatch->regexRoute, REG_EXTENDED))
  {
    printf("Could not compile regular expression.\n");
    return;
  };

  cursor = req->path;
  for (m = 0; m < maxMatches; m++)
  {
    if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))
      break; // No more matches

    unsigned int g = 0;
    unsigned int offset = 0;
    for (g = 0; g < maxGroups; g++)
    {
      if (groupArray[g].rm_so == (long long)(size_t)-1)
        break; // No more groups

      if (g == 0)
      {
        offset = groupArray[g].rm_eo;
        *match = 1;
      }
      else
      {
        int index = g - 1;
        req->paramValues[index] = malloc(sizeof(char) * (groupArray[g].rm_eo - groupArray[g].rm_so + 1));
        strncpy(req->paramValues[index], cursor + groupArray[g].rm_so, groupArray[g].rm_eo - groupArray[g].rm_so);
        req->paramValues[index][groupArray[g].rm_eo - groupArray[g].rm_so] = '\0';
      }

      // char cursorCopy[strlen(cursor) + 1];
      // strcpy(cursorCopy, cursor);
      // cursorCopy[groupArray[g].rm_eo] = 0;
    }
    cursor += offset;
  }

  regfree(&regexCompiled);
}

typedef char * (^getHashBlock)(char *key);
static getHashBlock reqQueryFactory(request_t *req)
{
  return Block_copy(^(char *key) {
    for (size_t i = 0; i != req->queryStringCount; ++i)
    {
      char *decodedKey = curl_easy_unescape(req->curl, req->queryStrings[i].key, req->queryStrings[i].keyLen, NULL); // curl_free ??
      if (strcmp(decodedKey, key) == 0)
      {
        curl_free(decodedKey);
        char *value = malloc(sizeof(char) * (req->queryStrings[i].valueLen + 1));
        memcpy(value, req->queryStrings[i].value, req->queryStrings[i].valueLen);
        value[req->queryStrings[i].valueLen] = '\0';
        char *decodedValue = curl_easy_unescape(req->curl, value, req->queryStrings[i].valueLen, NULL); // curl_free ??
        return decodedValue;
      }
    }
    return (char *)NULL;
  });
}

static session_t *reqSessionFactory(UNUSED request_t *req)
{
  session_t *session = malloc(sizeof(session_t));
  return session;
}

static getHashBlock reqCookieFactory(request_t *req)
{
  // TODO: replace with reading directly from req.cookiesString (pointer offsets and lengths)
  req->cookiesHash = hash_new();
  req->cookiesString = req->get("Cookie");
  if (req->cookiesString != NULL)
  {
    char *cookie = strtok(req->cookiesString, ";");
    int i = 0;
    while (cookie != NULL)
    {
      req->cookies[i] = cookie;
      cookie = strtok(NULL, ";");
      i++;
    }
    for (i = 0; i < 4096; i++)
    {
      if (req->cookies[i] == NULL)
        break;
      char *key = strtok(req->cookies[i], "=");
      char *value = strtok(NULL, "=");
      if (key[0] == ' ')
        key++;
      hash_set(req->cookiesHash, key, value);
    }
  }

  return Block_copy(^(char *key) {
    return (char *)hash_get(req->cookiesHash, key);
  });
}

typedef void * (^getMiddlewareBlock)(char *key);
static getMiddlewareBlock reqMiddlewareFactory(request_t *req)
{
  req->middlewareHash = hash_new();
  return Block_copy(^(char *key) {
    return hash_get(req->middlewareHash, key);
  });
}

typedef void (^getMiddlewareSetBlock)(char *key, void *middleware);
static getMiddlewareSetBlock reqMiddlewareSetFactory(request_t *req)
{
  return Block_copy(^(char *key, void *middleware) {
    return hash_set(req->middlewareHash, key, middleware);
  });
}

static getHashBlock reqBodyFactory(request_t *req)
{
  // TODO: replace with reading directly from req.bodyString (pointer offsets and lengths)
  req->bodyHash = hash_new();
  if (strncmp(req->method, "POST", 4) == 0 || strncmp(req->method, "PATCH", 5) == 0 || strncmp(req->method, "PUT", 3) == 0)
  {
    char *copy = strdup(req->rawRequest);
    req->bodyString = strstr(copy, "\r\n\r\n");

    if (req->bodyString && strlen(req->bodyString) > 4)
    {
      req->bodyString += 4;
      int i = 0;
      while (req->bodyString[i] != '\0')
      {
        if (req->bodyString[i] == '+')
          req->bodyString[i] = ' ';
        i++;
      }
      char *contentType = req->get("Content-Type");
      if (strncmp(contentType, "application/x-www-form-urlencoded", 33) == 0)
      {
        parseQueryStringHash(req->bodyHash, req->bodyString);
      }
      else if (strncmp(contentType, "application/json", 16) == 0)
      {
        printf("%s\n", req->bodyString);
      }
      else if (strncmp(contentType, "multipart/form-data", 20) == 0)
      {
        printf("%s\n", req->bodyString);
      }
      free(contentType);
    }
    else
    {
      req->bodyString = "";
    }
    free(copy);
  }
  return Block_copy(^(char *key) {
    return (char *)hash_get(req->bodyHash, key);
  });
}

static char *buildResponseString(char *body, response_t *res)
{
  if (res->get("Content-Type") == NULL)
    res->set("Content-Type", "text/html; charset=utf-8");

  char *contentLength = malloc(sizeof(char) * 20);
  sprintf(contentLength, "%zu", strlen(body));
  res->set("Content-Length", contentLength);

  res->set("Connection", "close");

  char *statusMessage = getStatusMessage(res->status);
  char *status = malloc(sizeof(char) * (strlen(statusMessage) + 5));
  sprintf(status, "%d %s", res->status, statusMessage);

  size_t customHeadersLen = 0;
  char customHeaders[4096];
  memset(customHeaders, 0, 4096);

  // TODO: replace with writing directly from res.headersString
  hash_each((hash_t *)res->headersHash, {
    size_t headersLen = strlen(key) + strlen(val) + 4;
    strncpy(customHeaders + customHeadersLen, key, strlen(key));
    customHeaders[customHeadersLen + strlen(key)] = ':';
    customHeaders[customHeadersLen + strlen(key) + 1] = ' ';
    strncpy(customHeaders + customHeadersLen + strlen(key) + 2, val, strlen(val));
    customHeaders[customHeadersLen + strlen(key) + strlen(val) + 2] = '\r';
    customHeaders[customHeadersLen + strlen(key) + strlen(val) + 3] = '\n';
    customHeadersLen += headersLen;
  });

  char *headers = malloc(sizeof(char) * (strlen("HTTP/1.1 \r\n\r\n") + strlen(status) + customHeadersLen + res->cookieHeadersLength + 1));
  sprintf(headers, "HTTP/1.1 %s\r\n%s%s\r\n", status, customHeaders, res->cookieHeaders);

  char *responseString = malloc(sizeof(char) * (strlen(headers) + strlen(body) + 1));
  strcpy(responseString, headers);
  strcat(responseString, body);
  free(status);
  free(headers);
  free(contentLength);
  return responseString;
}

typedef void (^sendBlock)(char *body);
static sendBlock sendFactory(client_t client, response_t *res)
{
  return Block_copy(^(char *body) {
    char *response = buildResponseString(body, res);
    write(client.socket, response, strlen(response));
    free(response);
  });
}

typedef void (^sendfBlock)(char *format, ...);
static sendfBlock sendfFactory(response_t *res)
{
  return Block_copy(^(char *format, ...) {
    char body[65536];
    va_list args;
    va_start(args, format);
    vsnprintf(body, 65536, format, args);
    char *response = buildResponseString(body, res);
    res->send(body);
    free(response);
    va_end(args);
  });
}

typedef void (^sendFileBlock)(char *body);
static sendFileBlock sendFileFactory(client_t client, request_t *req, response_t *res)
{
  return Block_copy(^(char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
      res->status = 404;
      res->sendf(errorHTML, req->path);
      return;
    }
    char *response = malloc(sizeof(char) * (strlen("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: \r\n\r\n") + 20));
    // TODO: mimetype
    // TODO: use res.set() and refactor header building
    sprintf(response, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n", fileSize(path));
    write(client.socket, response, strlen(response));
    // TODO: use sendfile
    char *buffer = malloc(4096);
    size_t bytesRead = fread(buffer, 1, 4096, file);
    while (bytesRead > 0)
    {
      write(client.socket, buffer, bytesRead);
      bytesRead = fread(buffer, 1, 4096, file);
    }
    free(buffer);
    free(response);
    fclose(file);
  });
}

typedef void (^setBlock)(char *headerKey, char *headerValue);
static setBlock setFactory(response_t *res)
{
  // TODO: replace hash with writing directly to res.headersString
  res->headersHash = hash_new();
  return Block_copy(^(char *headerKey, char *headerValue) {
    return hash_set(res->headersHash, headerKey, headerValue);
  });
}

typedef char * (^getBlock)(char *headerKey);
static getBlock getFactory(response_t *res)
{
  // TODO: replace hash with reading directly from res.headersString (pointer offsets and lengths)
  return Block_copy(^(char *headerKey) {
    return (char *)hash_get(res->headersHash, headerKey);
  });
}

static char *cookieOptsStringFromOpts(cookie_opts_t opts)
{
  char cookieOptsString[1024];
  memset(cookieOptsString, 0, 1024);
  int i = 0;
  if (opts.httpOnly)
  {
    strcpy(cookieOptsString + i, "; HttpOnly");
    i += strlen("; HttpOnly");
  }
  if (opts.secure)
  {
    strcpy(cookieOptsString + i, "; Secure");
    i += strlen("; HttpOnly; Secure");
  }
  if (opts.maxAge != 0)
  {
    char *maxAgeValue = malloc(sizeof(char) * (strlen("; Max-Age=") + 20));
    sprintf(maxAgeValue, "; Max-Age=%d", opts.maxAge);
    strcpy(cookieOptsString + i, maxAgeValue);
    i += strlen(maxAgeValue);
    free(maxAgeValue);
  }
  if (opts.expires != NULL)
  {
    char *expiresValue = malloc(sizeof(char) * (strlen("; Expires=") + 20));
    sprintf(expiresValue, "; Expires=%s", opts.expires);
    strcpy(cookieOptsString + i, expiresValue);
    i += strlen(expiresValue);
    free(expiresValue);
  }
  if (opts.domain != NULL)
  {
    char *domainValue = malloc(sizeof(char) * (strlen("; Domain=") + strlen(opts.domain) + 1));
    sprintf(domainValue, "; Domain=%s", opts.domain);
    strcpy(cookieOptsString + i, domainValue);
    i += strlen(domainValue);
    free(domainValue);
  }
  if (opts.path != NULL)
  {
    char *pathValue = malloc(sizeof(char) * (strlen("; Path=") + strlen(opts.path) + 1));
    sprintf(pathValue, "; Path=%s", opts.path);
    strcpy(cookieOptsString + i, pathValue);
    free(pathValue);
  }
  return strdup(cookieOptsString);
}

typedef void (^setCookie)(char *cookieKey, char *cookieValue, cookie_opts_t opts);
static setCookie cookieFactory(response_t *res)
{
  memset(res->cookieHeaders, 0, sizeof(res->cookieHeaders));
  res->cookieHeadersLength = 0;
  return Block_copy(^(char *key, char *value, cookie_opts_t opts) {
    char *cookieOptsString = cookieOptsStringFromOpts(opts);
    char *valueWithOptions = malloc(sizeof(char) * (strlen(value) + strlen(cookieOptsString) + 1));
    strcpy(valueWithOptions, value);
    strcat(valueWithOptions, cookieOptsString);

    size_t headersLen = strlen(key) + strlen(valueWithOptions) + 16;
    strncpy(res->cookieHeaders + res->cookieHeadersLength, "Set-Cookie", 10);
    res->cookieHeaders[res->cookieHeadersLength + 10] = ':';
    res->cookieHeaders[res->cookieHeadersLength + 11] = ' ';
    strncpy(res->cookieHeaders + res->cookieHeadersLength + 12, key, strlen(key));
    res->cookieHeaders[res->cookieHeadersLength + 12 + strlen(key)] = '=';
    strncpy(res->cookieHeaders + res->cookieHeadersLength + 13 + strlen(key), valueWithOptions, strlen(valueWithOptions));
    res->cookieHeaders[res->cookieHeadersLength + 13 + strlen(key) + strlen(valueWithOptions)] = ';';
    res->cookieHeaders[res->cookieHeadersLength + 14 + strlen(key) + strlen(valueWithOptions)] = '\r';
    res->cookieHeaders[res->cookieHeadersLength + 15 + strlen(key) + strlen(valueWithOptions)] = '\n';
    res->cookieHeadersLength += headersLen;

    free(cookieOptsString);
    free(valueWithOptions);
  });
}

typedef void (^urlBlock)(char *url);
static urlBlock locationFactory(request_t *req, response_t *res)
{
  return Block_copy(^(char *url) {
    if (strcmp(url, "back") == 0)
    {
      char *referer = req->get("Referer");
      if (referer != NULL)
      {
        res->set("Location", referer);
      }
      else
      {
        res->set("Location", "/");
      }
      return;
    }
    char *location = malloc(sizeof(char) * (strlen(req->path) + strlen(url) + 2));
    sprintf(location, "%s%s", req->path, url);
    res->set("Location", location);
  });
}

static urlBlock redirectFactory(UNUSED request_t *req, response_t *res)
{
  return Block_copy(^(char *url) {
    res->status = 302;
    res->location(url);
    res->sendf("Redirecting to %s", url);
    return;
  });
}

typedef struct route_handler_t
{
  char *method;
  char *path;
  int regex;
  param_match_t *paramMatch;
  requestHandler handler;
} route_handler_t;

typedef struct middleware_t
{
  char *path;
  middlewareHandler handler;
} middleware_t;

static route_handler_t *routeHandlers = NULL;
static int routeHandlerCount = 0;
static middleware_t *middlewares = NULL;
static int middlewareCount = 0;
cleanupHandler *cleanupBlocks = NULL;
static int servSock = -1;
static dispatch_queue_t serverQueue = NULL;

char *matchFilepath(request_t *req, char *path)
{
  regex_t regex;
  int reti;
  size_t nmatch = 2;
  regmatch_t pmatch[2];
  char *pattern = malloc(sizeof(char) * (strlen(path) + strlen("//(.*)") + 1));
  sprintf(pattern, "/%s/(.*)", path);
  char *buffer = malloc(sizeof(char) * (strlen(req->url) + 1));
  strcpy(buffer, req->path);
  reti = regcomp(&regex, pattern, REG_EXTENDED);
  if (reti)
  {
    fprintf(stderr, "Could not compile regex\n");
    exit(6);
  }
  reti = regexec(&regex, buffer, nmatch, pmatch, 0);
  if (reti == 0)
  {
    char *fileName = buffer + pmatch[1].rm_so;
    fileName[pmatch[1].rm_eo - pmatch[1].rm_so] = 0;
    char *filePath = malloc(sizeof(char) * (strlen(fileName) + strlen(".//") + strlen(path) + 1));
    sprintf(filePath, "./%s/%s", path, fileName);
    regfree(&regex);
    free(buffer);
    free(pattern);
    return filePath;
  }
  else
  {
    regfree(&regex);
    free(buffer);
    free(pattern);
    return NULL;
  }
}

static void initRouteHandlers()
{
  routeHandlers = malloc(sizeof(route_handler_t));
}

static void addRouteHandler(char *method, char *path, requestHandler handler)
{
  int regex = strchr(path, ':') != NULL;
  routeHandlers = realloc(routeHandlers, sizeof(route_handler_t) * (routeHandlerCount + 1));
  route_handler_t routeHandler = {
      .method = method,
      .path = path,
      .regex = regex,
      .paramMatch = regex ? paramMatch(path) : NULL,
      .handler = handler,
  };
  routeHandlers[routeHandlerCount++] = routeHandler;
}

static void initMiddlewareHandlers()
{
  middlewares = malloc(sizeof(middleware_t));
  cleanupBlocks = malloc(sizeof(cleanupHandler));
}

static void addMiddlewareHandler(middlewareHandler handler)
{
  middlewares = realloc(middlewares, sizeof(middleware_t) * (middlewareCount + 1));
  cleanupBlocks = realloc(cleanupBlocks, sizeof(cleanupHandler) * (middlewareCount + 1));
  middlewares[middlewareCount++] = (middleware_t){.handler = handler};
}

static void runMiddleware(int index, request_t *req, response_t *res, void (^next)())
{
  if (index < middlewareCount)
  {
    req->middlewareStackIndex = index;
#ifdef MIDDLEWARE_DEBUG
    printf("Running middleware %d\n", index);
#endif // MIDDLEWARE_DEBUG
    void (^cleanup)(cleanupHandler) = ^(cleanupHandler cleanupBlock) {
#ifdef MIDDLEWARE_DEBUG
      printf("Adding cleanup block %d\n", index);
#endif // MIDDLEWARE_DEBUG
      cleanupBlocks[index] = cleanupBlock;
    };
    middlewares[index].handler(
        req, res, ^{
          runMiddleware(index + 1, req, res, next);
        },
        cleanup);
  }
  else
  {
    next();
  }
}

static void initServerQueue()
{
  serverQueue = dispatch_queue_create("serverQueue", DISPATCH_QUEUE_CONCURRENT);
}

static client_t acceptClientConnection()
{
  int clntSock = -1;
  struct sockaddr_in echoClntAddr;
  unsigned int clntLen = sizeof(echoClntAddr);
#ifdef CLIENT_NET_DEBUG
  printf("\nAccepting client...\n");
#endif // CLIENT_NET_DEBUG
  if ((clntSock = accept(servSock, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
  {
    // perror("accept() failed");
    return (client_t){.socket = -1, .ip = NULL};
  }
#ifdef CLIENT_NET_DEBUG
  printf("Client accepted\n");
#endif // CLIENT_NET_DEBUG

  // Make the socket non-blocking
  if (fcntl(clntSock, F_SETFL, O_NONBLOCK) < 0)
  {
    // perror("fcntl() failed");
    shutdown(clntSock, SHUT_RDWR);
    close(clntSock);
  }

  char *client_ip = inet_ntoa(echoClntAddr.sin_addr);

#ifdef CLIENT_NET_DEBUG
  printf("Client connected from %s\n", client_ip);
#endif // CLIENT_NET_DEBUG

  return (client_t){.socket = clntSock, .ip = client_ip};
}

static void initServerSocket()
{
  if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printf("socket() failed");
  }

  int flag = 1;
  if (-1 == setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)))
  {
    perror("setsockopt fail");
  }
}

static void freeMiddlewares()
{
  for (int i = 0; i < middlewareCount; i++)
  {
    free(middlewares[i].path);
    Block_release(middlewares[i].handler);
  }
  free(middlewares);
}

static void paramMatchFree(param_match_t *paramMatch)
{
  for (int i = 0; i < paramMatch->count; i++)
  {
    free(paramMatch->keys[i]);
  }
  free(paramMatch->keys);
  free(paramMatch->regexRoute);
}

static void freeRouteHandlers()
{
  for (int i = 0; i < routeHandlerCount; i++)
  {
    if (routeHandlers[i].regex)
    {
      paramMatchFree(routeHandlers[i].paramMatch);
    }
    Block_release(routeHandlers[i].handler);
  }
  free(routeHandlers);
}

static void closeServer()
{
  printf("\nClosing server...\n");
  freeRouteHandlers();
  freeMiddlewares();
  close(servSock);
  dispatch_release(serverQueue);
  exit(EXIT_SUCCESS);
}

static void initServerListen(int port)
{
  struct sockaddr_in servAddr;
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(port);

  // TODO: run in background
  // TODO: save pid
  // TODO: TLS/SSL support

  if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
  {
    printf("bind() failed\n");
    closeServer();
  }

  // Make the socket non-blocking
  if (fcntl(servSock, F_SETFL, O_NONBLOCK) < 0)
  {
    shutdown(servSock, SHUT_RDWR);
    close(servSock);
    perror("fcntl() failed");
    closeServer();
  }

  if (listen(servSock, 10000) < 0)
  {
    printf("listen() failed");
    closeServer();
  }
};

static request_t parseRequest(client_t client)
{
  request_t req = {.url = NULL, .queryString = "", .path = NULL, .method = NULL, .rawRequest = NULL};

  char buffer[4096];
  memset(buffer, 0, sizeof(buffer));
  char *method, *url;
  int parseBytes, minorVersion;
  size_t bufferLen = 0, prevBufferLen = 0, methodLen, urlLen;
  ssize_t readBytes;

  // TODO: timeout support
  while (1)
  {
#ifdef CLIENT_NET_DEBUG
    printf("\nWaiting for request...\n");
#endif // CLIENT_NET_DEBUG
    while ((readBytes = read(client.socket, buffer + bufferLen, sizeof(buffer) - bufferLen)) == -1)
      ;
#ifdef CLIENT_NET_DEBUG
    printf("\nRequest received\n");
#endif // CLIENT_NET_DEBUG
    if (readBytes <= 0)
      return req;
    prevBufferLen = bufferLen;
    bufferLen += readBytes;
    req.numHeaders = sizeof(req.headers) / sizeof(req.headers[0]);
    parseBytes = phr_parse_request(buffer, bufferLen, (const char **)&method, &methodLen, (const char **)&url, &urlLen,
                                   &minorVersion, req.headers, &req.numHeaders, prevBufferLen);
    if (parseBytes > 0)
    {
      req.get = ^(char *headerKey) {
        for (size_t i = 0; i != req.numHeaders; ++i)
        {

          char *key = malloc(sizeof(char) * (req.headers[i].name_len + 1));
          sprintf(key, "%.*s", (int)req.headers[i].name_len, req.headers[i].name);
          char *value = malloc(sizeof(char) * (req.headers[i].value_len + 1));
          sprintf(value, "%.*s", (int)req.headers[i].value_len, req.headers[i].value);
          if (strcasecmp(key, headerKey) == 0)
          {
            free(key);
            return value;
          }
          free(key);
          free(value);
        }
        return (char *)NULL;
      };
      char *contentLength = req.get("Content-Length");
      if (contentLength != NULL && parseBytes == readBytes && contentLength[0] != '0')
      {
        while ((read(client.socket, buffer + bufferLen, sizeof(buffer) - bufferLen)) == -1)
          ;
      }
      free(contentLength);
      break;
    }
    else if (parseBytes == -1)
      return req;
    assert(parseBytes == -2);
    if (bufferLen == sizeof(buffer))
      return req;
  }

  req.curl = curl_easy_init();

  req.rawRequest = buffer;

  req.method = malloc(sizeof(char) * (methodLen + 1));
  memcpy(req.method, method, methodLen);
  req.method[methodLen] = '\0';

  req.url = malloc(sizeof(char) * (urlLen + 1));
  memcpy(req.url, url, urlLen);
  req.url[urlLen] = '\0';

  char *copy = strdup(req.url);
  char *queryStringStart = strchr(copy, '?');

  if (queryStringStart)
  {
    int queryStringLen = strlen(queryStringStart + 1);
    req.queryString = malloc(sizeof(char) * (queryStringLen + 1));
    memcpy(req.queryString, queryStringStart + 1, queryStringLen);
    req.queryString[queryStringLen] = '\0';
    *queryStringStart = '\0';
    parseQueryString(req.queryString, req.queryString + queryStringLen, req.queryStrings, &req.queryStringCount,
                     sizeof(req.queryStrings) / sizeof(req.queryStrings[0]));
  }

  req.path = malloc(sizeof(char) * (strlen(copy) + 1));
  memcpy(req.path, copy, strlen(copy));
  req.path[strlen(copy)] = '\0';

  req.query = reqQueryFactory(&req);
  req.body = reqBodyFactory(&req);
  req.session = reqSessionFactory(&req);
  req.cookie = reqCookieFactory(&req);
  req.m = reqMiddlewareFactory(&req);
  req.mSet = reqMiddlewareSetFactory(&req);

  req.middlewareStackIndex = 0;

  char *_method = req.body("_method");
  if (_method)
  {
    toUpper(_method);
    if (strcmp(_method, "PUT") == 0 || strcmp(_method, "DELETE") == 0 || strcmp(_method, "PATCH") == 0)
      req.method = _method;
  }

  free(copy);

  return req;
}

static route_handler_t matchRouteHandler(request_t *req)
{
  for (int i = 0; i < routeHandlerCount; i++)
  {
    if (strcmp(routeHandlers[i].method, req->method) != 0)
      continue;
    req->paramMatch = routeHandlers[i].paramMatch;
    if (req->paramMatch != NULL)
    {
      req->paramValues = malloc(sizeof(char *) * req->paramMatch->count);
      for (int j = 0; j < req->paramMatch->count; j++)
      {
        req->paramValues[j] = NULL;
      }
      int match = 0;
      routeMatch(req, &match);
      if (match)
      {
        // TODO: replace with pointer offsets and lengths
        req->paramsHash = hash_new();
        for (int k = 0; k < req->paramMatch->count; k++)
        {
          hash_set(req->paramsHash, req->paramMatch->keys[k], req->paramValues[k]);
        }
        req->params = Block_copy(^(char *key) {
          // TODO: replace with reading directly from req->paramsString (pointer offsets and lengths)
          return (char *)hash_get(req->paramsHash, key);
        });
        match = 0;
        return routeHandlers[i];
      }
    }

    if (strcmp(routeHandlers[i].method, req->method) == 0 && strcmp(routeHandlers[i].path, req->path) == 0)
    {
      return routeHandlers[i];
    }
  }
  return (route_handler_t){.method = NULL, .path = NULL, .handler = NULL};
}

static void freeRequest(request_t req)
{
  for (int i = 1; i <= req.middlewareStackIndex; i++)
  {
#ifdef MIDDLEWARE_DEBUG
    printf("Freeing middleware %d\n", i);
#endif // MIDDLEWARE_DEBUG
    cleanupBlocks[i]((request_t *)&req);
  }
  free(req.method);
  free(req.path);
  free(req.url);
  free(req.session);
  if (req.paramMatch != NULL)
  {
    for (int i = 0; i < req.paramMatch->count; i++)
    {
      free(req.paramMatch->keys[i]);
      free(req.paramValues[i]);
    }
  }
  free(req.paramMatch);
  free(req.paramValues);
  free(req.cookiesString);
  // if (req.bodyString != NULL && strlen(req.bodyString) > 0)
  //   free(req.bodyString);
  if (strlen(req.queryString) > 0)
    free(req.queryString);
  hash_free(req.paramsHash);
  hash_free(req.bodyHash);
  hash_free(req.cookiesHash);
  hash_free(req.middlewareHash);
  Block_release(req.query);
  Block_release(req.params);
  Block_release(req.body);
  Block_release(req.cookie);
  Block_release(req.m);
  Block_release(req.mSet);
  curl_easy_cleanup(req.curl);
}

static void freeResponse(response_t res)
{
  hash_free(res.headersHash);
  Block_release(res.send);
  Block_release(res.sendFile);
  Block_release(res.sendf);
  Block_release(res.set);
  Block_release(res.get);
  Block_release(res.cookie);
  Block_release(res.location);
  Block_release(res.redirect);
}

static void closeClientConnection(client_t client)
{
#ifdef CLIENT_NET_DEBUG
  printf("\nClosing client connection...\n");
#endif // CLIENT_NET_DEBUG
  shutdown(client.socket, SHUT_RDWR);
  close(client.socket);
}

static response_t buildResponse(client_t client, request_t *req)
{
  response_t res;
  res.status = 200;
  res.send = sendFactory(client, &res);
  res.sendf = sendfFactory(&res);
  res.sendFile = sendFileFactory(client, req, &res);
  res.set = setFactory(&res);
  res.get = getFactory(&res);
  res.cookie = cookieFactory(&res);
  res.location = locationFactory(req, &res);
  res.redirect = redirectFactory(req, &res);
  return res;
}

#ifdef __linux__
/*

An synchronous, single-threaded implementation of the client handler that uses select().

TODO: implement a multi-threaded version that uses epoll()

*/
static void initClientAcceptEventHandler()
{
  fd_set readFdSet;
  int maxClients = 30;
  client_t clients[30];

  int activity;

  for (int i = 0; i < maxClients; i++)
  {
    client_t client = {.socket = 0};
    clients[i] = client;
  }

  struct timeval waitd = {0, 1};

#ifdef CLIENT_NET_DEBUG
  printf("\nWaiting for client connections...\n");
#endif // CLIENT_NET_DEBUG
  while (1)
  {
    FD_ZERO(&readFdSet);
    FD_SET(servSock, &readFdSet);
    int maxSocket = servSock;

    for (int i = 0; i < maxClients; i++)
    {
      if (clients[i].socket > 0)
        FD_SET(clients[i].socket, &readFdSet);

      if (clients[i].socket > maxSocket)
        maxSocket = clients[i].socket;
    }

    activity = select(maxSocket + 1, &readFdSet, NULL, NULL, &waitd);

    if (FD_ISSET(servSock, &readFdSet))
    {
#ifdef CLIENT_NET_DEBUG
      printf("\nRead event on servSock\n");
#endif // CLIENT_NET_DEBUG
      client_t client = acceptClientConnection();
      if (client.socket < 0)
        continue;

      for (int i = 0; i < maxClients; i++)
      {
        if (clients[i].socket == 0)
        {
          clients[i] = client;
          break;
        }
      }
    }

    for (int i = 0; i < maxClients; i++)
    {
      client_t client = clients[i];

      if (FD_ISSET(client.socket, &readFdSet))
      {
        request_t req = parseRequest(client);

        if (req.method == NULL)
        {
          closeClientConnection(client);
          freeRequest(req);
          return;
        }

        __block response_t res = buildResponse(client, &req);

        runMiddleware(0, &req, &res, ^{
          route_handler_t routeHandler = matchRouteHandler((request_t *)&req);
          if (routeHandler.handler == NULL)
          {
            res.status = 404;
            res.sendf(errorHTML, req.path);
          }
          else
          {
            routeHandler.handler((request_t *)&req, (response_t *)&res);
          }
        });

        closeClientConnection(client);
        freeRequest(req);
        freeResponse(res);
        clients[i].socket = 0;
      }
    }
  }
}
#elif __MACH__
/*

A concurrent, multi-threaded implementation of the client handler that uses dispatch_sources.

For an unknown reason this implementation is not working on linux.

*/
static void initClientAcceptEventHandler()
{
  dispatch_source_t acceptSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, servSock, 0, serverQueue);

  dispatch_source_set_event_handler(acceptSource, ^{
#ifdef CLIENT_NET_DEBUG
    printf("\nRead event on servSock\n");
#endif // CLIENT_NET_DEBUG
    const unsigned long numPendingConnections = dispatch_source_get_data(acceptSource);
    for (unsigned long i = 0; i < numPendingConnections; i++)
    {
      client_t client = acceptClientConnection();
      if (client.socket < 0)
        continue;

      dispatch_source_t readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, client.socket, 0, serverQueue);
      dispatch_source_set_event_handler(readSource, ^{
#ifdef CLIENT_NET_DEBUG
        printf("\nGot client read\n");
#endif // CLIENT_NET_DEBUG
        request_t req = parseRequest(client);

        if (req.method == NULL)
        {
          closeClientConnection(client);
          freeRequest(req);
          dispatch_source_cancel(readSource);
          dispatch_release(readSource);
          return;
        }

        __block response_t res = buildResponse(client, &req);

        runMiddleware(0, &req, &res, ^{
          route_handler_t routeHandler = matchRouteHandler((request_t *)&req);
          if (routeHandler.handler == NULL)
          {
            res.status = 404;
            res.sendf(errorHTML, req.path);
          }
          else
          {
            routeHandler.handler((request_t *)&req, (response_t *)&res);
          }
        });

        closeClientConnection(client);
        freeRequest(req);
        freeResponse(res);
        dispatch_source_cancel(readSource);
        dispatch_release(readSource);
      });
#ifdef CLIENT_NET_DEBUG
      printf("\nWaiting for client reads...\n");
#endif // CLIENT_NET_DEBUG
      dispatch_resume(readSource);
    }
  });

#ifdef CLIENT_NET_DEBUG
  printf("\nWaiting for client connections...\n");
#endif // CLIENT_NET_DEBUG
  dispatch_resume(acceptSource);
}
#endif

middlewareHandler expressStatic(char *path)
{
  return Block_copy(^(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler)) {
    char *filePath = matchFilepath(req, path);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    if (filePath != NULL)
    {
      res->sendFile(filePath);
      free(filePath);
    }
    else
    {
      next();
    }
  });
}

char *generateUuid()
{
  char *guid = malloc(sizeof(char) * 37);
  if (guid == NULL)
  {
    return NULL;
  }
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, guid);
  return guid;
}

middlewareHandler memSessionMiddlewareFactory()
{
  __block hash_t *memSessionStore = hash_new();

  dispatch_queue_t memSessionQueue = dispatch_queue_create("memSessionQueue", NULL);

  return Block_copy(^(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler)) {
    req->session->uuid = req->cookie("sessionUuid");
    if (req->session->uuid == NULL)
    {
      req->session->uuid = generateUuid();
      cookie_opts_t opts = {.path = "/", .maxAge = 60 * 60 * 24 * 365, .httpOnly = true};
      res->cookie("sessionUuid", req->session->uuid, opts);
    }

    if (hash_has(memSessionStore, req->session->uuid))
    {
      req->session->store = hash_get(memSessionStore, req->session->uuid);
    }
    else
    {
      req->session->store = hash_new(); // is not being freed
      dispatch_sync(memSessionQueue, ^{
        hash_set(memSessionStore, req->session->uuid, req->session->store);
      });
    }

    req->session->get = ^(char *key) {
      return hash_get(req->session->store, key);
    };

    req->session->set = ^(char *key, void *value) {
      hash_set(req->session->store, key, value);
    };

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    next();
  });
}

app_t express()
{
  initMiddlewareHandlers();
  initRouteHandlers();
  initServerQueue();
  initServerSocket();

  if (signal(SIGINT, closeServer) == SIG_ERR)
    ;

  app_t app;

  app.listen = ^(int port, void (^handler)()) {
    initServerListen(port);
    dispatch_async(serverQueue, ^{
      initClientAcceptEventHandler();
    });
    handler();
    dispatch_main();
  };

  app.get = ^(char *path, requestHandler handler) {
    addRouteHandler("GET", path, handler);
  };

  app.post = ^(char *path, requestHandler handler) {
    addRouteHandler("POST", path, handler);
  };

  app.put = ^(char *path, requestHandler handler) {
    addRouteHandler("PUT", path, handler);
  };

  app.patch = ^(char *path, requestHandler handler) {
    addRouteHandler("PATCH", path, handler);
  };

  app.delete = ^(char *path, requestHandler handler) {
    addRouteHandler("DELETE", path, handler);
  };

  app.use = ^(middlewareHandler handler) {
    addMiddlewareHandler(handler);
  };

  return app;
};
