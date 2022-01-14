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
#include "express.h"

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

static void parseQueryString(hash_t *hash, char *string)
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
  }
}

typedef struct client_t
{
  int socket;
  char *ip;
} client_t;

typedef struct param_match_t
{
  char *regexRoute;
  char **keys;
  int count;
} param_match_t;

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

  pm->regexRoute = malloc(strlen(regexRoute) + 1);
  strcpy(pm->regexRoute, regexRoute);

  return pm;
}

void routeMatch(char *path, param_match_t *pm, char **values, int *match)
{
  char *source = path;
  char *regexString = pm->regexRoute;

  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;
  char *cursor;

  if (regcomp(&regexCompiled, regexString, REG_EXTENDED))
  {
    printf("Could not compile regular expression.\n");
    return;
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

      if (g == 0)
      {
        offset = groupArray[g].rm_eo;
        *match = 1;
      }
      else
      {
        int index = g - 1;
        values[index] = malloc(sizeof(char) * (groupArray[g].rm_eo - groupArray[g].rm_so + 1));
        strncpy(values[index], cursor + groupArray[g].rm_so, groupArray[g].rm_eo - groupArray[g].rm_so);
        values[index][groupArray[g].rm_eo - groupArray[g].rm_so] = '\0';
      }

      char cursorCopy[strlen(cursor) + 1];
      strcpy(cursorCopy, cursor);
      cursorCopy[groupArray[g].rm_eo] = 0;
    }
    cursor += offset;
  }

  regfree(&regexCompiled);
}

typedef char * (^getHashBlock)(char *key);
static getHashBlock reqQueryFactory(request_t *req)
{
  return Block_copy(^(char *key) {
    return (char *)hash_get(req->queryHash, key);
  });
}

static getHashBlock reqGetHeaderFactory(request_t *req)
{
  return Block_copy(^(char *key) {
    return (char *)hash_get(req->headersHash, key);
  });
}

static getHashBlock reqParamFactory(request_t *req)
{
  return Block_copy(^(char *key) {
    return (char *)hash_get(req->paramsHash, key);
  });
}

static session_t *reqSessionFactory(UNUSED request_t *req)
{
  session_t *session = malloc(sizeof(session_t));
  return session;
}

static getHashBlock reqCookieFactory(UNUSED request_t *req)
{
  req->cookiesHash = hash_new();
  char *cookiesString = hash_get(req->headersHash, "Cookie");
  if (cookiesString != NULL)
  {
    char *cookie = strtok(cookiesString, ";");
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

  return Block_copy(^(UNUSED char *key) {
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
  req->bodyHash = hash_new();
  if (strncmp(req->method, "POST", 4) == 0)
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
      if (strncmp(req->get("Content-Type"), "application/x-www-form-urlencoded", 33) == 0)
      {
        parseQueryString(req->bodyHash, req->bodyString);
      }
      else if (strncmp(req->get("Content-Type"), "application/json", 16) == 0)
      {
        printf("%s\n", req->bodyString);
      }
      else if (strncmp(req->get("Content-Type"), "multipart/form-data", 20) == 0)
      {
        printf("%s\n", req->bodyString);
      }
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
  char *contentType = "text/html; charset=utf-8";
  char *contentLength = malloc(sizeof(char) * 20);
  sprintf(contentLength, "%zu", strlen(body));
  char *statusMessage = getStatusMessage(res->status);
  char *status = malloc(sizeof(char) * (strlen(statusMessage) + 5));
  sprintf(status, "%d %s", res->status, statusMessage);

  size_t customHeadersLen = 0;
  char customHeaders[4096];
  memset(customHeaders, 0, 4096);

  // TODO: make hashing function pointers passed in to app during init
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

  hash_each((hash_t *)res->cookiesHash, {
    size_t headersLen = strlen(key) + strlen(val) + 16;
    strncpy(customHeaders + customHeadersLen, "Set-Cookie", 10);
    customHeaders[customHeadersLen + 10] = ':';
    customHeaders[customHeadersLen + 11] = ' ';
    strncpy(customHeaders + customHeadersLen + 12, key, strlen(key));
    customHeaders[customHeadersLen + 12 + strlen(key)] = '=';
    strncpy(customHeaders + customHeadersLen + 13 + strlen(key), val, strlen(val));
    customHeaders[customHeadersLen + 13 + strlen(key) + strlen(val)] = ';';
    customHeaders[customHeadersLen + 14 + strlen(key) + strlen(val)] = '\r';
    customHeaders[customHeadersLen + 15 + strlen(key) + strlen(val)] = '\n';
    customHeadersLen += headersLen;
  });

  // TODO: add support for other content types
  char *headers = malloc(sizeof(char) * (strlen("HTTP/1.1 \r\nContent-Type: \r\nContent-Length: \r\n\r\n") + strlen(status) + strlen(contentType) + strlen(contentLength) + customHeadersLen + 1));
  sprintf(headers, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %s\r\n%s\r\n", status, contentType, contentLength, customHeaders);

  char *responseString = malloc(sizeof(char) * (strlen(headers) + strlen(body) + 1));
  strcpy(responseString, headers);
  strcat(responseString, body);
  free(status);
  free(headers);
  free(contentLength);
  return responseString;
}

typedef void (^sendBlock)(const char *format, ...);
static sendBlock sendFactory(client_t client, response_t *res)
{
  return Block_copy(^(const char *format, ...) {
    char body[65536];
    va_list args;
    va_start(args, format);
    vsnprintf(body, 65536, format, args);
    char *response = buildResponseString(body, res);
    write(client.socket, response, strlen(response));
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
      res->send(errorHTML, req->path);
      return;
    }
    char *response = malloc(sizeof(char) * (strlen("HTTP/1.1 200 OK\r\nContent-Length: \r\n\r\n") + 20));
    // TODO: mimetype
    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", fileSize(path));
    write(client.socket, response, strlen(response));
    char *bufferfer = malloc(4096);
    size_t bytesRead = fread(bufferfer, 1, 4096, file);
    while (bytesRead > 0)
    {
      write(client.socket, bufferfer, bytesRead);
      bytesRead = fread(bufferfer, 1, 4096, file);
    }
    free(bufferfer);
    free(response);
    fclose(file);
  });
}

typedef void (^setBlock)(char *headerKey, char *headerValue);
static setBlock setFactory(response_t *res)
{
  res->headersHash = hash_new();
  return Block_copy(^(char *headerKey, char *headerValue) {
    return hash_set(res->headersHash, headerKey, headerValue);
  });
}

static setBlock cookieFactory(response_t *res)
{
  res->cookiesHash = hash_new();
  return Block_copy(^(char *key, char *value) {
    return hash_set(res->cookiesHash, key, value);
  });
}

typedef void (^urlBlock)(char *url);
static urlBlock locationFactory(request_t *req, response_t *res)
{
  return Block_copy(^(char *url) {
    if (strcmp(url, "back") == 0)
    {
      res->set("Location", req->get("Referer"));
      return;
    }
    char *location = malloc(sizeof(char) * (strlen(req->path) + strlen(url) + 2));
    sprintf(location, "%s%s", req->path, url);
    res->set("Location", location);
    free(location);
  });
}

static urlBlock redirectFactory(UNUSED request_t *req, response_t *res)
{
  return Block_copy(^(char *url) {
    res->status = 302;
    res->location(url);
    res->send("Redirecting to %s", url);
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
  char *bufferfer = malloc(sizeof(char) * (strlen(req->url) + 1));
  strcpy(bufferfer, req->path);
  reti = regcomp(&regex, pattern, REG_EXTENDED);
  if (reti)
  {
    fprintf(stderr, "Could not compile regex\n");
    exit(6);
  }
  reti = regexec(&regex, bufferfer, nmatch, pmatch, 0);
  if (reti == 0)
  {
    char *fileName = bufferfer + pmatch[1].rm_so;
    fileName[pmatch[1].rm_eo - pmatch[1].rm_so] = 0;
    char *file_path = malloc(sizeof(char) * (strlen(fileName) + strlen(".//") + strlen(path) + 1));
    sprintf(file_path, "./%s/%s", path, fileName);
    return file_path;
  }
  else
  {
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
}

static void addMiddlewareHandler(middlewareHandler handler)
{
  middlewares = realloc(middlewares, sizeof(middleware_t) * (middlewareCount + 1));
  middlewares[middlewareCount++] = (middleware_t){.handler = handler};
}

static void runMiddleware(int index, request_t *req, response_t *res, void (^next)())
{
  if (index < middlewareCount)
  {
    middlewares[index].handler(req, res, ^{
      runMiddleware(index + 1, req, res, next);
    });
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
  if ((clntSock = accept(servSock, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
  {
    // perror("accept() failed");
    return (client_t){.socket = -1, .ip = NULL};
  }

  // Make the socket non-blocking
  if (fcntl(clntSock, F_SETFL, O_NONBLOCK) < 0)
  {
    // perror("fcntl() failed");
    shutdown(clntSock, SHUT_RDWR);
    close(clntSock);
  }

  char *client_ip = inet_ntoa(echoClntAddr.sin_addr);

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

static void closeServer()
{
  printf("\nClosing server...\n");
  free(routeHandlers);
  free(middlewares);
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
  struct phr_header headers[100];
  size_t bufferLen = 0, prevBufferLen = 0, methodLen, urlLen, numHeaders;
  ssize_t readBytes;

  // TODO: timeout support
  while (1)
  {
    while ((readBytes = read(client.socket, buffer + bufferLen, sizeof(buffer) - bufferLen)) == -1)
      ;
    if (readBytes <= 0)
      return req;
    prevBufferLen = bufferLen;
    bufferLen += readBytes;
    numHeaders = sizeof(headers) / sizeof(headers[0]);
    parseBytes = phr_parse_request(buffer, bufferLen, (const char **)&method, &methodLen, (const char **)&url, &urlLen,
                                   &minorVersion, headers, &numHeaders, prevBufferLen);
    if (parseBytes > 0)
    {
      if (method[0] == 'P' && parseBytes == readBytes)
        while ((read(client.socket, buffer + bufferLen, sizeof(buffer) - bufferLen)) == -1)
          ;
      break;
    }
    else if (parseBytes == -1)
      return req;
    assert(parseBytes == -2);
    if (bufferLen == sizeof(buffer))
      return req;
  }

  req.rawRequest = buffer;

  req.headersHash = hash_new();
  for (size_t i = 0; i != numHeaders; ++i)
  {
    char *key = malloc(headers[i].name_len + 1);
    sprintf(key, "%.*s", (int)headers[i].name_len, headers[i].name);
    char *value = malloc(headers[i].value_len + 1);
    sprintf(value, "%.*s", (int)headers[i].value_len, headers[i].value);
    hash_set(req.headersHash, key, value);
  }

  req.method = malloc(methodLen + 1);
  memcpy(req.method, method, methodLen);
  req.method[methodLen] = '\0';

  req.url = malloc(urlLen + 1);
  memcpy(req.url, url, urlLen);
  req.url[urlLen] = '\0';

  char *copy = strdup(req.url);
  char *queryStringStart = strchr(copy, '?');

  if (queryStringStart)
  {
    int queryStringLen = strlen(queryStringStart + 1);
    req.queryString = malloc(queryStringLen + 1);
    memcpy(req.queryString, queryStringStart + 1, queryStringLen);
    req.queryString[queryStringLen] = '\0';
    *queryStringStart = '\0';
    req.queryHash = hash_new();
    parseQueryString(req.queryHash, req.queryString);
  }

  req.path = malloc(strlen(copy) + 1);
  memcpy(req.path, copy, strlen(copy));
  req.path[strlen(copy)] = '\0';

  req.get = reqGetHeaderFactory(&req);
  req.query = reqQueryFactory(&req);
  req.body = reqBodyFactory(&req);
  req.session = reqSessionFactory(&req);
  req.cookie = reqCookieFactory(&req);
  req.m = reqMiddlewareFactory(&req);
  req.mSet = reqMiddlewareSetFactory(&req);

  char *_method = req.body("_method");
  if (_method)
  {
    toUpper(_method);
    if (strcmp(_method, "PUT") == 0 || strcmp(_method, "DELETE") == 0)
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
    param_match_t *pm = routeHandlers[i].paramMatch;
    if (pm != NULL)
    {
      char *values[pm->count];
      req->paramValues = values;
      for (int j = 0; j < pm->count; j++)
      {
        values[j] = NULL;
      }
      int match = 0;
      routeMatch(req->path, pm, req->paramValues, &match);
      if (match)
      {
        req->paramsHash = hash_new();
        for (int k = 0; k < pm->count; k++)
        {
          hash_set(req->paramsHash, pm->keys[k], req->paramValues[k]);
        }
        req->params = reqParamFactory(req);
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

static void closeClientConnection(client_t client)
{
  shutdown(client.socket, SHUT_RDWR);
  close(client.socket);
}

static response_t buildResponse(client_t client, request_t *req)
{
  response_t res;
  res.status = 200;
  res.send = sendFactory(client, &res);
  res.sendFile = sendFileFactory(client, req, &res);
  res.set = setFactory(&res);
  res.cookie = cookieFactory(&res);
  res.location = locationFactory(req, &res);
  res.redirect = redirectFactory(req, &res);
  return res;
}

static void initClientAcceptEventHandler()
{
  dispatch_source_t acceptSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, servSock, 0, serverQueue);

  dispatch_source_set_event_handler(acceptSource, ^{
    const unsigned long numPendingConnections = dispatch_source_get_data(acceptSource);
    for (unsigned long i = 0; i < numPendingConnections; i++)
    {
      client_t client = acceptClientConnection();
      if (client.socket < 0)
        continue;

      request_t req = parseRequest(client);

      if (req.method == NULL)
      {
        closeClientConnection(client);
        return;
      }

      __block response_t res = buildResponse(client, &req);

      runMiddleware(0, &req, &res, ^{
        route_handler_t routeHandler = matchRouteHandler((request_t *)&req);
        if (routeHandler.handler == NULL)
        {
          res.status = 404;
          res.send(errorHTML, req.path);
        }
        else
        {
          routeHandler.handler((request_t *)&req, (response_t *)&res);
        }
      });

      closeClientConnection(client);
    }
  });

  dispatch_resume(acceptSource);
}

middlewareHandler expressStatic(char *path)
{
  return Block_copy(^(request_t *req, response_t *res, void (^next)()) {
    char *filePath = matchFilepath(req, path);
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

static char *generateUuid()
{
  char *guid = malloc(37);
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

  return Block_copy(^(request_t *req, response_t *res, void (^next)()) {
    char *sessionUuid = req->cookie("sessionUuid");
    if (sessionUuid == NULL)
    {
      sessionUuid = generateUuid();
      res->cookie("sessionUuid", sessionUuid);
    }
    req->session->uuid = sessionUuid;

    if (hash_has(memSessionStore, sessionUuid))
    {
      req->session->store = hash_get(memSessionStore, sessionUuid);
    }
    else
    {
      req->session->store = hash_new();
      hash_set(memSessionStore, sessionUuid, req->session->store);
    }

    req->session->get = ^(char *key) {
      return hash_get(req->session->store, key);
    };

    req->session->set = ^(char *key, void *value) {
      hash_set(req->session->store, key, value);
    };

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
    initClientAcceptEventHandler();
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

  app.delete = ^(char *path, requestHandler handler) {
    addRouteHandler("DELETE", path, handler);
  };

  app.use = ^(middlewareHandler handler) {
    addMiddlewareHandler(handler);
  };

  return app;
};
