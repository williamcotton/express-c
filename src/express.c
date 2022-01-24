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
#include <MegaMimes/MegaMimes.h>
#include <uuid/uuid.h>
#include <sys/errno.h>
#ifdef __linux__
#include <sys/epoll.h>
#include <bsd/string.h>
#include <pthread.h>
#endif
#include "express.h"

// #define CLIENT_NET_DEBUG

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
cleanupHandler *middlewareCleanupBlocks = NULL;
static int appCleanupCount = 0;
appCleanupHandler *appCleanupBlocks = NULL;
static int servSock = -1;
static dispatch_queue_t serverQueue = NULL;

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

static void parseQueryString(const char *buf, const char *bufEnd, key_value_t *keyValues, size_t *keyValueCount, size_t max)
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
      if (*keyValueCount < max)
      {
        keyValues[*keyValueCount].key = keyStart;
        keyValues[*keyValueCount].keyLen = keyLen;
        keyValues[*keyValueCount].value = valueStart;
        keyValues[*keyValueCount].valueLen = valueLen;
        (*keyValueCount)++;
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
      strlcpy(cursorCopy, cursor, groupArray[g].rm_eo + 1);

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
        strlcpy(key, cursorCopy + groupArray[g].rm_so, groupArray[g].rm_eo - groupArray[g].rm_so + 1);
        pm->keys[m] = key;
      }
    }
    cursor += offset;
  }

  sprintf(regexRoute + strlen(regexRoute), "%s", cursor);

  regfree(&regexCompiled);

  size_t regexLen = strlen(regexRoute) + 1;
  pm->regexRoute = malloc(sizeof(char) * regexLen);
  strncpy(pm->regexRoute, regexRoute, regexLen);

  return pm;
}

typedef void * (^mallocBlock)(size_t);
static mallocBlock reqMallocFactory(request_t *req)
{
  req->mallocCount = 0;
  return Block_copy(^(size_t size) {
    void *ptr = malloc(size);
    req->mallocs[req->mallocCount++] = (req_malloc_t){.ptr = ptr};
    return ptr;
  });
}

typedef void * (^copyBlock)(void *);
static copyBlock reqBlockCopyFactory(request_t *req)
{
  req->blockCopyCount = 0;
  return Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    req->blockCopies[req->blockCopyCount++] = (req_block_copy_t){.ptr = ptr};
    return ptr;
  });
}

typedef char * (^getBlock)(char *key);
static getBlock reqQueryFactory(request_t *req)
{
  return Block_copy(^(char *key) {
    for (size_t i = 0; i != req->queryKeyValueCount; ++i)
    {
      size_t keyLen = strlen(key);
      char *decodedKey = curl_easy_unescape(req->curl, req->queryKeyValues[i].key, req->queryKeyValues[i].keyLen, NULL); // curl_free ??
      if (strncmp(decodedKey, key, keyLen) == 0)
      {
        curl_free(decodedKey);
        char *value = malloc(sizeof(char) * (req->queryKeyValues[i].valueLen + 1));
        strlcpy(value, req->queryKeyValues[i].value, req->queryKeyValues[i].valueLen + 1);
        char *decodedValue = curl_easy_unescape(req->curl, value, req->queryKeyValues[i].valueLen, NULL); // curl_free ??
        free(value);
        return decodedValue;
      }
      curl_free(decodedKey);
    }
    return (char *)NULL;
  });
}

static session_t *reqSessionFactory(UNUSED request_t *req)
{
  session_t *session = malloc(sizeof(session_t));
  return session;
}

void routeMatch(const char *path, const char *regexRoute, key_value_t *paramKeyValues, int *match)
{
  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;

  if (regcomp(&regexCompiled, regexRoute, REG_EXTENDED))
  {
    printf("Could not compile regular expression.\n");
    return;
  };

  const char *cursor = path;
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
        paramKeyValues[index].value = cursor + groupArray[g].rm_so;
        paramKeyValues[index].valueLen = groupArray[g].rm_eo - groupArray[g].rm_so;
      }
    }
    cursor += offset;
  }

  regfree(&regexCompiled);
}

static getBlock reqParamsFactory(request_t *req)
{
  req->pathMatch = "";
  for (int i = 0; i < routeHandlerCount; i++)
  {
    if (routeHandlers[i].paramMatch != NULL)
    {
      int match = 0;
      routeMatch(req->path, routeHandlers[i].paramMatch->regexRoute, req->paramKeyValues, &match);
      if (match)
      {
        req->pathMatch = routeHandlers[i].path;
        req->paramKeyValueCount = routeHandlers[i].paramMatch->count;
        for (int j = 0; j < routeHandlers[i].paramMatch->count; j++)
        {
          req->paramKeyValues[j].key = routeHandlers[i].paramMatch->keys[j];
          req->paramKeyValues[j].keyLen = strlen(routeHandlers[i].paramMatch->keys[j]);
        }
        break;
      }
    }
  }
  return Block_copy(^(char *key) {
    for (size_t j = 0; j < req->paramKeyValueCount; j++)
    {
      size_t keyLen = strlen(key);
      if (strncmp(req->paramKeyValues[j].key, key, keyLen) == 0)
      {
        char *value = malloc(sizeof(char) * (req->paramKeyValues[j].valueLen + 1));
        strlcpy(value, req->paramKeyValues[j].value, req->paramKeyValues[j].valueLen + 1);
        return value;
      }
    }
    return (char *)NULL;
  });
}

static getBlock reqCookieFactory(request_t *req)
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

static getBlock reqBodyFactory(request_t *req)
{
  if (strncmp(req->method, "POST", 4) == 0 || strncmp(req->method, "PATCH", 5) == 0 || strncmp(req->method, "PUT", 3) == 0)
  {
    req->rawRequestBody = strdup(req->rawRequest);
    req->bodyString = strstr(req->rawRequestBody, "\r\n\r\n");
    if (req->bodyString && strlen(req->bodyString) > 4)
    {
      req->bodyString += 4;
      char *contentType = req->get("Content-Type");
      if (strncmp(contentType, "application/x-www-form-urlencoded", 33) == 0)
      {
        size_t bodyStringLen = strlen(req->bodyString);
        req->bodyKeyValueCount = 0;
        parseQueryString(req->bodyString, req->bodyString + bodyStringLen, req->bodyKeyValues, &req->bodyKeyValueCount,
                         sizeof(req->bodyKeyValues) / sizeof(req->bodyKeyValues[0]));
      }
      else if (strncmp(contentType, "application/json", 16) == 0)
      {
        printf("application/json: %s\n", req->bodyString);
      }
      else if (strncmp(contentType, "multipart/form-data", 20) == 0)
      {
        printf("multipart/form-data: %s\n", req->bodyString);
      }
      free(contentType);
    }
    else
    {
      req->bodyString = "";
    }
  }
  return Block_copy(^(char *key) {
    for (size_t i = 0; i != req->bodyKeyValueCount; ++i)
    {
      size_t keyLen = strlen(key);
      char *decodedKey = curl_easy_unescape(req->curl, req->bodyKeyValues[i].key, req->bodyKeyValues[i].keyLen, NULL); // curl_free ??
      if (strncmp(decodedKey, key, keyLen) == 0)
      {
        char *value = malloc(sizeof(char) * (req->bodyKeyValues[i].valueLen + 1));
        strlcpy(value, req->bodyKeyValues[i].value, req->bodyKeyValues[i].valueLen + 1);
        int j = 0;
        while (value[j] != '\0')
        {
          if (value[j] == '+')
            value[j] = ' ';
          j++;
        }
        char *decodedValue = curl_easy_unescape(req->curl, value, req->bodyKeyValues[i].valueLen, NULL); // curl_free ??
        free(value);
        curl_free(decodedKey);
        return decodedValue;
      }
      curl_free(decodedKey);
    }
    return (char *)NULL;
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

  size_t headersLen = strlen(headers) + 1;
  size_t bodyLen = strlen(body);
  char *responseString = malloc(sizeof(char) * (headersLen + bodyLen));
  strncpy(responseString, headers, headersLen);
  strncat(responseString, body, bodyLen);
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

static sendBlock sendFileFactory(client_t client, request_t *req, response_t *res)
{
  return Block_copy(^(char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
      res->status = 404;
      res->sendf(errorHTML, req->path);
      return;
    }
    char *mimetype = (char *)getMegaMimeType((const char *)path);
    char *response = malloc(sizeof(char) * (strlen(mimetype) + strlen("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: \r\nContent-Length: \r\n\r\n") + 20));
    // TODO: use res.set() and refactor header building
    sprintf(response, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: %s\r\nContent-Length: %zu\r\n\r\n", mimetype, fileSize(path));
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
  size_t i = 0;
  if (opts.httpOnly)
  {
    strncpy(cookieOptsString + i, "; HttpOnly", 10);
    i += strlen("; HttpOnly");
  }
  if (opts.secure)
  {
    strncpy(cookieOptsString + i, "; Secure", 8);
    i += strlen("; HttpOnly; Secure");
  }
  if (opts.maxAge != 0)
  {
    size_t maxAgeLen = strlen("; Max-Age=") + 20;
    char *maxAgeValue = malloc(sizeof(char) * maxAgeLen);
    sprintf(maxAgeValue, "; Max-Age=%d", opts.maxAge);
    strncpy(cookieOptsString + i, maxAgeValue, maxAgeLen);
    i += strlen(maxAgeValue);
    free(maxAgeValue);
  }
  if (opts.expires != NULL)
  {
    size_t expiresLen = strlen("; Expires=") + 20;
    char *expiresValue = malloc(sizeof(char) * expiresLen);
    sprintf(expiresValue, "; Expires=%s", opts.expires);
    strncpy(cookieOptsString + i, expiresValue, expiresLen);
    i += strlen(expiresValue);
    free(expiresValue);
  }
  if (opts.domain != NULL)
  {
    size_t domainLen = strlen("; Domain=") + strlen(opts.domain) + 1;
    char *domainValue = malloc(sizeof(char) * domainLen);
    sprintf(domainValue, "; Domain=%s", opts.domain);
    strncpy(cookieOptsString + i, domainValue, domainLen);
    i += strlen(domainValue);
    free(domainValue);
  }
  if (opts.path != NULL)
  {
    size_t pathLen = strlen("; Path=") + strlen(opts.path) + 1;
    char *pathValue = malloc(sizeof(char) * pathLen);
    sprintf(pathValue, "; Path=%s", opts.path);
    strncpy(cookieOptsString + i, pathValue, pathLen);
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
    size_t valueLen = strlen(value) + 1;
    size_t cookieStringOptsLen = strlen(cookieOptsString);
    char *valueWithOptions = malloc(sizeof(char) * (valueLen + cookieStringOptsLen));
    strncpy(valueWithOptions, value, valueLen);
    strncat(valueWithOptions, cookieOptsString, cookieStringOptsLen);

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
    if (strncmp(url, "back", 4) == 0)
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
    char *location = req->malloc(sizeof(char) * (strlen(req->path) + strlen(url) + 2));
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

char *matchFilepath(request_t *req, char *path)
{
  regex_t regex;
  int reti;
  size_t nmatch = 2;
  regmatch_t pmatch[2];
  size_t patternLen = strlen(path) + strlen("//(.*)") + 1;
  char *pattern = malloc(sizeof(char) * patternLen);
  snprintf(pattern, patternLen, "/%s/(.*)", path);
  size_t pathLen = strlen(req->path) + 1;
  char *buffer = malloc(sizeof(char) * pathLen);
  strncpy(buffer, req->path, pathLen);
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
    size_t filePathLen = strlen(fileName) + strlen(".//") + strlen(path) + 1;
    char *filePath = malloc(sizeof(char) * filePathLen);
    snprintf(filePath, filePathLen, "./%s/%s", path, fileName);
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

static void initAppCleanupBlocks()
{
  appCleanupBlocks = malloc(sizeof(appCleanupHandler));
}

static void initMiddlewareHandlers()
{
  middlewares = malloc(sizeof(middleware_t));
  middlewareCleanupBlocks = malloc(sizeof(cleanupHandler));
}

static void addMiddlewareHandler(middlewareHandler handler)
{
  middlewares = realloc(middlewares, sizeof(middleware_t) * (middlewareCount + 1));
  middlewareCleanupBlocks = realloc(middlewareCleanupBlocks, sizeof(cleanupHandler) * (middlewareCount + 1));
  middlewares[middlewareCount++] = (middleware_t){.handler = handler};
}

static void addCleanupHandler(appCleanupHandler handler)
{
  appCleanupBlocks = realloc(appCleanupBlocks, sizeof(appCleanupHandler) * (appCleanupCount + 1));
  appCleanupBlocks[appCleanupCount++] = handler;
}

static void runMiddleware(int index, request_t *req, response_t *res, void (^next)())
{
  if (index < middlewareCount)
  {
    req->middlewareStackIndex = index;
    void (^cleanup)(cleanupHandler) = ^(cleanupHandler cleanupBlock) {
      middlewareCleanupBlocks[index] = cleanupBlock;
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
    perror("socket() failed");
  }

  int flag = 1;
  if (-1 == setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)))
  {
    perror("setsockopt() failed");
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
      free(routeHandlers[i].paramMatch);
    }
    Block_release(routeHandlers[i].handler);
  }
  free(routeHandlers);
}

void closeServer(int status)
{
  printf("\nClosing server...\n");
  freeRouteHandlers();
  freeMiddlewares();
  close(servSock);
  dispatch_release(serverQueue);
  for (int i = 0; i < appCleanupCount; i++)
  {
    appCleanupBlocks[i]();
  }
  exit(status);
}

static void initServerListen(int port)
{
  struct sockaddr_in servAddr;
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(port);

  // TODO: TLS/SSL support

  if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
  {
    printf("bind() failed\n");
    closeServer(EXIT_FAILURE);
  }

  // Make the socket non-blocking
  if (fcntl(servSock, F_SETFL, O_NONBLOCK) < 0)
  {
    shutdown(servSock, SHUT_RDWR);
    close(servSock);
    perror("fcntl() failed");
    closeServer(EXIT_FAILURE);
  }

  if (listen(servSock, 10000) < 0)
  {
    printf("listen() failed");
    closeServer(EXIT_FAILURE);
  }
};

static request_t parseRequest(client_t client)
{
  request_t req = {.url = NULL, .queryString = "", .path = NULL, .method = NULL, .rawRequest = NULL};

  req.malloc = reqMallocFactory(&req);
  req.blockCopy = reqBlockCopyFactory(&req);

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
          if (strncmp(req.headers[i].name, headerKey, req.headers[i].name_len) == 0)
          {
            char *value = malloc(sizeof(char) * (req.headers[i].value_len + 1));
            sprintf(value, "%.*s", (int)req.headers[i].value_len, req.headers[i].value);
            return value;
          }
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
  strlcpy(req.method, method, methodLen + 1);

  req.url = malloc(sizeof(char) * (urlLen + 1));
  strlcpy(req.url, url, urlLen + 1);

  char *path = req.url;
  char *queryStringStart = strchr(path, '?');

  if (queryStringStart)
  {
    size_t queryStringLen = strlen(queryStringStart + 1);
    req.queryString = malloc(sizeof(char) * (queryStringLen + 1));
    strlcpy(req.queryString, queryStringStart + 1, queryStringLen + 1);
    *queryStringStart = '\0';
    req.queryKeyValueCount = 0;
    parseQueryString(req.queryString, req.queryString + queryStringLen, req.queryKeyValues, &req.queryKeyValueCount,
                     sizeof(req.queryKeyValues) / sizeof(req.queryKeyValues[0]));
  }

  size_t pathLen = strlen(path) + 1;
  req.path = malloc(sizeof(char) * pathLen);
  snprintf(req.path, pathLen, "%s", path);

  req.params = reqParamsFactory(&req);
  req.query = reqQueryFactory(&req);
  req.body = reqBodyFactory(&req);
  req.session = reqSessionFactory(&req);
  req.cookie = reqCookieFactory(&req);
  req.m = reqMiddlewareFactory(&req);
  req.mSet = reqMiddlewareSetFactory(&req);

  req.middlewareStackIndex = 0;

  req._method = req.body("_method");
  if (req._method)
  {
    toUpper(req._method);
    if (strcmp(req._method, "PUT") == 0 || strcmp(req._method, "DELETE") == 0 || strcmp(req._method, "PATCH") == 0)
    {
      free(req.method);
      req.method = req._method;
    }
  }

  return req;
}

static route_handler_t matchRouteHandler(request_t *req)
{
  for (int i = 0; i < routeHandlerCount; i++)
  {
    size_t methodLen = strlen(routeHandlers[i].method);

    if (strncmp(routeHandlers[i].method, req->method, methodLen) != 0)
      continue;

    if (strcmp(routeHandlers[i].path, req->pathMatch) == 0)
      return routeHandlers[i];

    if (strcmp(routeHandlers[i].path, req->path) == 0)
      return routeHandlers[i];
  }
  return (route_handler_t){.method = NULL, .path = NULL, .handler = NULL};
}

static void freeRequest(request_t req)
{
  for (int i = 1; i <= req.middlewareStackIndex; i++)
  {
    middlewareCleanupBlocks[i]((request_t *)&req);
  }
  if (req._method != NULL)
    curl_free(req._method);
  else
    free(req.method);
  free(req.path);
  free(req.url);
  free(req.session);
  if (req.paramMatch != NULL)
  {
    for (int i = 0; i < req.paramMatch->count; i++)
    {
      free(req.paramMatch->keys[i]);
    }
  }
  free(req.paramMatch);
  free(req.cookiesString);
  free(req.rawRequestBody);
  if (strlen(req.queryString) > 0)
    free(req.queryString);
  hash_free(req.cookiesHash);
  hash_free(req.middlewareHash);
  for (int i = 0; i < req.mallocCount; i++)
  {
    free(req.mallocs[i].ptr);
  }
  for (int i = 0; i < req.blockCopyCount; i++)
  {
    Block_release(req.blockCopies[i].ptr);
  }
  Block_release(req.query);
  Block_release(req.params);
  Block_release(req.body);
  Block_release(req.cookie);
  Block_release(req.m);
  Block_release(req.mSet);
  Block_release(req.malloc);
  Block_release(req.blockCopy);
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

A multi-threaded implementation of the client handler that uses epoll().

*/

#define THREAD_NUM 4
#define MAX_EVENTS 64

typedef enum req_status_t
{
  READING,
  ENDED
} req_status_t;

typedef struct http_status_t
{
  client_t client;
  req_status_t reqStatus;
} http_status_t;

typedef struct client_thread_args_t
{
  int serverFd;
  int epollFd;
} client_thread_args_t;

void *thread(void *args)
{
  struct epoll_event *events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
  if (events == NULL)
  {
    perror("malloc() failed");
  }
  struct epoll_event ev;
  int epollFd = ((client_thread_args_t *)args)->epollFd;
  int serverFd = ((client_thread_args_t *)args)->serverFd;

  int nfds;
  while (1)
  {
    nfds = epoll_wait(epollFd, events, MAX_EVENTS, -1);

    if (nfds <= 0)
    {
      // perror("epoll_wait() failed");
      continue;
    }
    for (int n = 0; n < nfds; ++n)
    {
      if (events[n].data.fd == serverFd)
      {
        while (1)
        {
          client_t client = acceptClientConnection();
          if (client.socket < 0)
          {
            if (errno == EAGAIN | errno == EWOULDBLOCK)
            {
              break;
            }
            else
            {
              // perror("accept() failed");
              break;
            }
          }

#ifdef CLIENT_NET_DEBUG
          printf("\nRead event on servSock\n");
#endif // CLIENT_NET_DEBUG

          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

          http_status_t *status = malloc(sizeof(http_status_t));
          status->client = client;
          status->reqStatus = READING;

          ev.data.ptr = status;

          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, client.socket, &ev) < 0)
          {
            // perror("epoll_ctl() failed");
            continue;
          }
        }
      }
      else
      {
        http_status_t *status = (http_status_t *)events[n].data.ptr;
        if (status->reqStatus == READING)
        {
#ifdef CLIENT_NET_DEBUG
          printf("\nGot client read\n");
#endif // CLIENT_NET_DEBUG

          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
          ev.data.ptr = status;

          client_t client = status->client;

          request_t req = parseRequest(client);

          if (req.method == NULL)
          {
            closeClientConnection(client);
            freeRequest(req);
            continue;
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

          status->reqStatus = ENDED;

          if (epoll_ctl(epollFd, EPOLL_CTL_MOD, client.socket, &ev) < 0)
          {
            // perror("epoll_ctl() failed");
            freeRequest(req);
            freeResponse(res);
            closeClientConnection(client);
            continue;
          }

          freeRequest(req);
          freeResponse(res);
          closeClientConnection(client);
        }
        else if (status->reqStatus == ENDED)
        {
          free(status);
        }
      }
    }
  }

  free(events);
}

static void initClientAcceptEventHandler()
{

  pthread_t threads[THREAD_NUM];

  int epollFd = epoll_create1(0);
  if (epollFd < 0)
  {
    fprintf(stderr, "error while creating epoll fd\n");
    closeServer(EXIT_FAILURE);
  }

  client_thread_args_t threadArgs;
  threadArgs.epollFd = epollFd;
  threadArgs.serverFd = servSock;

  struct epoll_event epollEvent;
  epollEvent.events = EPOLLIN | EPOLLET;
  epollEvent.data.fd = servSock;

  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, servSock, &epollEvent) < 0)
  {
    fprintf(stderr, "error while adding listen fd to epoll inst\n");
    closeServer(EXIT_FAILURE);
  }

  for (int i = 0; i < THREAD_NUM; ++i)
  {
    if (pthread_create(&threads[i], NULL, thread, &threadArgs) < 0)
    {
      fprintf(stderr, "error while creating %d thread\n", i);
      closeServer(EXIT_FAILURE);
    }
  }

#ifdef CLIENT_NET_DEBUG
  printf("\nWaiting for client reads...\n");
#endif // CLIENT_NET_DEBUG

#ifdef CLIENT_NET_DEBUG
  printf("\nWaiting for client connections...\n");
#endif // CLIENT_NET_DEBUG
  thread(&threadArgs);
}
/*

An synchronous, single-threaded implementation of the client handler that uses select().

Keeping around for reference.

*/
UNUSED static void initClientAcceptEventHandlerPoll()
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

char *cwdFullPath(char *path)
{
  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  size_t fullPathLen = strlen(cwd) + strlen(path) + 2;
  char *fullPath = malloc(sizeof(char) * fullPathLen); // leak
  snprintf(fullPath, fullPathLen, "%s/%s", cwd, path);
  return fullPath;
}

middlewareHandler expressStatic(char *path, char *fullPath)
{
  return Block_copy(^(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    char *filePath = matchFilepath(req, path);

    char *rPath = realpath(filePath, NULL);
    int isTraversal = rPath && strncmp(rPath, fullPath, strlen(fullPath)) != 0;

    if (rPath)
      free(rPath);

    if (isTraversal)
    {
      res->status = 403;
      res->sendf(errorHTML, req->path);
      return;
    }

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

int writePid(char *pidFile)
{
  char buf[100];
  int fd = open(pidFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  snprintf(buf, 100, "%ld\n", (long)getpid());
  return (unsigned long)write(fd, buf, strlen(buf)) == (unsigned long)strlen;
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

middlewareHandler memSessionMiddlewareFactory(hash_t *memSessionStore, dispatch_queue_t memSessionQueue)
{
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
  initAppCleanupBlocks();
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

  app.cleanup = ^(appCleanupHandler handler) {
    addCleanupHandler(handler);
  };

  return app;
};
