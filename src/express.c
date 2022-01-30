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

/* Private */

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

static size_t getFileSize(const char *filePath)
{
  struct stat st;
  stat(filePath, &st);
  return st.st_size;
}

static char *getFileName(const char *filePath)
{
  char *fileName = strrchr(filePath, '/');
  if (fileName)
  {
    return fileName + 1;
  }
  return (char *)filePath;
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

static param_match_t *paramMatch(UNUSED const char *basePath, const char *route)
{
  param_match_t *pm = malloc(sizeof(param_match_t));
  pm->keys = malloc(sizeof(char *));
  pm->count = 0;
  char regexRoute[4096];
  regexRoute[0] = '\0';
  sprintf(regexRoute + strlen(regexRoute), "%s", basePath);
  const char *source = route;
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

  cursor = (char *)source;
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

  size_t regesRouteLen = strlen(regexRoute) + 2;
  pm->regexRoute = malloc(sizeof(char) * (regesRouteLen));
  snprintf(pm->regexRoute, regesRouteLen, "^%s", regexRoute);

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

typedef char * (^getBlock)(const char *key);
static getBlock reqQueryFactory(request_t *req)
{
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i != req->queryKeyValueCount; ++i)
    {
      size_t keyLen = strlen(key);
      char *decodedKey = curl_easy_unescape(req->curl, req->queryKeyValues[i].key, req->queryKeyValues[i].keyLen, NULL);
      if (strncmp(decodedKey, key, keyLen) == 0)
      {
        curl_free(decodedKey);
        char *value = malloc(sizeof(char) * (req->queryKeyValues[i].valueLen + 1));
        strlcpy(value, req->queryKeyValues[i].value, req->queryKeyValues[i].valueLen + 1);
        char *decodedValue = curl_easy_unescape(req->curl, value, req->queryKeyValues[i].valueLen, NULL);
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

static void routeMatch(const char *path, const char *regexRoute, key_value_t *paramKeyValues, int *match)
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

static void collectRegexRouteHandlers(router_t *router, route_handler_t *regExRouteHandlers, int *regExRouteHandlerCount)
{
  for (int i = 0; i < router->routeHandlerCount; i++)
  {
    if (router->routeHandlers[i].paramMatch != NULL)
    {
      regExRouteHandlers[*regExRouteHandlerCount] = router->routeHandlers[i];
      (*regExRouteHandlerCount)++;
    }
  }

  for (int i = 0; i < router->routerCount; ++i)
    collectRegexRouteHandlers(router->routers[i], regExRouteHandlers, regExRouteHandlerCount);
}

static getBlock reqParamsFactory(request_t *req, router_t *baseRouter)
{
  route_handler_t regExRouteHandlers[4096];
  int regExRouteHandlerCount = 0;
  collectRegexRouteHandlers(baseRouter, regExRouteHandlers, &regExRouteHandlerCount);
  req->pathMatch = "";
  for (int i = 0; i < regExRouteHandlerCount; i++)
  {
    int match = 0;
    routeMatch(req->path, regExRouteHandlers[i].paramMatch->regexRoute, req->paramKeyValues, &match);
    if (match)
    {
      int pathMatchLen = strlen(regExRouteHandlers[i].basePath) + strlen(regExRouteHandlers[i].path) + 1;
      req->pathMatch = malloc(sizeof(char) * pathMatchLen);
      snprintf((char *)req->pathMatch, pathMatchLen, "%s%s", regExRouteHandlers[i].basePath, regExRouteHandlers[i].path);
      req->paramKeyValueCount = regExRouteHandlers[i].paramMatch->count;
      for (int j = 0; j < regExRouteHandlers[i].paramMatch->count; j++)
      {
        req->paramKeyValues[j].key = regExRouteHandlers[i].paramMatch->keys[j];
        req->paramKeyValues[j].keyLen = strlen(regExRouteHandlers[i].paramMatch->keys[j]);
      }
      break;
    }
  }
  return Block_copy(^(const char *key) {
    for (size_t j = 0; j < req->paramKeyValueCount; j++)
    {
      size_t keyLen = strlen(key);
      if (strncmp(req->paramKeyValues[j].key, key, keyLen) == 0)
      {
        char *value = malloc(sizeof(char) * (req->paramKeyValues[j].valueLen + 1));
        strlcpy(value, req->paramKeyValues[j].value, req->paramKeyValues[j].valueLen + 1);
        return (char *)value;
      }
    }
    return (char *)NULL;
  });
}

static getBlock reqCookieFactory(request_t *req)
{
  // TODO: replace with reading directly from req.cookiesString (pointer offsets and lengths)
  req->cookiesHash = hash_new();
  req->cookiesString = (char *)req->get("Cookie");
  if (req->cookiesString != NULL)
  {
    char *cookie = strtok((char *)req->cookiesString, ";");
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
      char *key = strtok((char *)req->cookies[i], "=");
      char *value = strtok(NULL, "=");
      if (key[0] == ' ')
        key++;
      hash_set(req->cookiesHash, key, value);
    }
  }

  return Block_copy(^(const char *key) {
    return (char *)hash_get(req->cookiesHash, (char *)key);
  });
}

typedef void * (^getMiddlewareBlock)(const char *key);
static getMiddlewareBlock reqMiddlewareFactory(request_t *req)
{
  req->middlewareHash = hash_new();
  return Block_copy(^(const char *key) {
    return hash_get(req->middlewareHash, (char *)key);
  });
}

typedef void (^getMiddlewareSetBlock)(const char *key, void *middleware);
static getMiddlewareSetBlock reqMiddlewareSetFactory(request_t *req)
{
  return Block_copy(^(const char *key, void *middleware) {
    return hash_set(req->middlewareHash, (char *)key, middleware);
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
      char *contentType = (char *)req->get("Content-Type");
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
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i != req->bodyKeyValueCount; ++i)
    {
      size_t keyLen = strlen(key);
      char *decodedKey = curl_easy_unescape(req->curl, req->bodyKeyValues[i].key, req->bodyKeyValues[i].keyLen, NULL);
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
        char *decodedValue = curl_easy_unescape(req->curl, value, req->bodyKeyValues[i].valueLen, NULL);
        free(value);
        curl_free(decodedKey);
        return decodedValue;
      }
      curl_free(decodedKey);
    }
    return (char *)NULL;
  });
}

static char *buildResponseString(const char *body, response_t *res)
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
  hash_each((hash_t *)res->headersHash,
            {
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

typedef void (^sendBlock)(const char *body);
static sendBlock resSendFactory(client_t client, response_t *res)
{
  return Block_copy(^(const char *body) {
    char *response = buildResponseString(body, res);
    res->didSend = 1;
    write(client.socket, response, strlen(response));
    free(response);
  });
}

typedef void (^sendfBlock)(const char *format, ...);
static sendfBlock resSendfFactory(response_t *res)
{
  return Block_copy(^(const char *format, ...) {
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

static sendBlock resSendFileFactory(client_t client, request_t *req, response_t *res)
{
  return Block_copy(^(const char *path) {
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
    sprintf(response, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: %s\r\nContent-Length: %zu\r\n\r\n", mimetype, getFileSize(path));
    res->didSend = 1;
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

typedef void (^sendStatusBlock)(int status);
static sendStatusBlock resSendStatusFactory(response_t *res)
{
  return Block_copy(^(int status) {
    res->status = status;
    res->send(getStatusMessage(status));
  });
}

typedef void (^typeBlock)(const char *type);
static typeBlock resTypeFactory(response_t *res)
{
  return Block_copy(^(const char *type) {
    const char *mimetype = getMegaMimeType(type);
    res->set("Content-Type", mimetype);
  });
}

typedef void (^jsonBlock)(const char *json);
static jsonBlock resJsonFactory(response_t *res)
{
  return Block_copy(^(const char *json) {
    res->set("Content-Type", "application/json");
    res->send(json);
  });
}

typedef void (^downloadBlock)(const char *filePath, const char *name);
static downloadBlock resDownloadFactory(response_t *res)
{
  return Block_copy(^(const char *filePath, const char *fileName) {
    if (fileName == NULL)
      fileName = (char *)getFileName(filePath);

    char *contentDisposition = malloc(sizeof(char) * (strlen("Content-Disposition: attachment; filename=\"\"\r\n") + strlen(fileName) + 1));
    sprintf(contentDisposition, "Content-Disposition: attachment; filename=\"%s\"\r\n", fileName);
    res->set("Content-Disposition", contentDisposition);
    res->sendFile(filePath);
  });
}

typedef void (^setBlock)(const char *headerKey, const char *headerValue);
static setBlock resSetFactory(response_t *res)
{
  // TODO: replace hash with writing directly to res.headersString
  res->headersHash = hash_new();
  return Block_copy(^(const char *headerKey, const char *headerValue) {
    return hash_set(res->headersHash, (char *)headerKey, (char *)headerValue);
  });
}

static getBlock resGetFactory(response_t *res)
{
  // TODO: replace hash with reading directly from res.headersString (pointer offsets and lengths)
  return Block_copy(^(const char *headerKey) {
    return (char *)hash_get(res->headersHash, (char *)headerKey);
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
    i += strlen("; Secure");
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

typedef void (^setCookie)(const char *cookieKey, const char *cookieValue, cookie_opts_t opts);
static setCookie resCookieFactory(response_t *res)
{
  memset(res->cookieHeaders, 0, sizeof(res->cookieHeaders));
  res->cookieHeadersLength = 0;
  return Block_copy(^(const char *key, const char *value, cookie_opts_t opts) {
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

typedef void (^urlBlock)(const char *url);
static urlBlock resLocationFactory(request_t *req, response_t *res)
{
  return Block_copy(^(const char *url) {
    if (strncmp(url, "back", 4) == 0)
    {
      const char *referer = req->get("Referer");
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

static urlBlock resRedirectFactory(UNUSED request_t *req, response_t *res)
{
  return Block_copy(^(const char *url) {
    res->status = 302;
    res->location(url);
    res->sendf("Redirecting to %s", url);
    return;
  });
}

static char *matchFilepath(request_t *req, const char *path)
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
    return NULL;
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

static void runMiddleware(int index, request_t *req, response_t *res, router_t *router, void (^next)())
{
  if (index < router->middlewareCount)
  {
    void (^cleanup)(cleanupHandler) = ^(UNUSED cleanupHandler cleanupBlock) {
      req->middlewareCleanupBlocks = realloc(req->middlewareCleanupBlocks, sizeof(cleanupHandler *) * (req->middlewareStackCount + 1));
      req->middlewareCleanupBlocks[req->middlewareStackCount++] = (void *)cleanupBlock;
    };
    router->middlewares[index].handler(
        req, res, ^{
          runMiddleware(index + 1, req, res, router, next);
        },
        cleanup);
  }
  else
  {
    next();
  }
}

static client_t acceptClientConnection(int serverSocket)
{
  int clntSock = -1;
  struct sockaddr_in echoClntAddr;
  unsigned int clntLen = sizeof(echoClntAddr);
  if ((clntSock = accept(serverSocket, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
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

static void initServerSocket(int *serverSocket)
{
  if ((*serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    perror("socket() failed");
  }

  int flag = 1;
  if (-1 == setsockopt(*serverSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)))
  {
    perror("setsockopt() failed");
  }
}

static void freeMiddlewares(middleware_t *middlewares, int middlewareCount)
{
  for (int i = 0; i < middlewareCount; i++)
  {
    free((void *)middlewares[i].path);
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

static void freeRouteHandlers(route_handler_t *routeHandlers, int routeHandlerCount)
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

static int initServerListen(int port, int *serverSocket)
{
  struct sockaddr_in servAddr;
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(port);

  // TODO: TLS/SSL support

  if (bind(*serverSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
  {
    printf("bind() failed\n");
    return -1;
  }

  // Make the socket non-blocking
  if (fcntl(*serverSocket, F_SETFL, O_NONBLOCK) < 0)
  {
    shutdown(*serverSocket, SHUT_RDWR);
    close(*serverSocket);
    perror("fcntl() failed");
    return -1;
  }

  if (listen(*serverSocket, 10000) < 0)
  {
    printf("listen() failed");
    return -1;
  }

  return 0;
};

static request_t buildRequest(client_t client, router_t *baseRouter)
{
  request_t req = {.url = NULL, .queryString = "", .path = NULL, .method = NULL, .rawRequest = NULL};

  req.malloc = reqMallocFactory(&req);
  req.blockCopy = reqBlockCopyFactory(&req);

  req.middlewareCleanupBlocks = malloc(sizeof(cleanupHandler *));

  char buffer[4096];
  memset(buffer, 0, sizeof(buffer));
  char *method, *originalUrl;
  int parseBytes, minorVersion;
  size_t bufferLen = 0, prevBufferLen = 0, methodLen, originalUrlLen;
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
    req.numHeaders = sizeof(req.headers) / sizeof(req.headers[0]);
    parseBytes = phr_parse_request(buffer, bufferLen, (const char **)&method, &methodLen, (const char **)&originalUrl, &originalUrlLen,
                                   &minorVersion, req.headers, &req.numHeaders, prevBufferLen);
    if (parseBytes > 0)
    {
      req.get = Block_copy(^(const char *headerKey) {
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
      });
      char *contentLength = (char *)req.get("Content-Length");
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
  strlcpy((char *)req.method, method, methodLen + 1);

  req.originalUrl = malloc(sizeof(char) * (originalUrlLen + 1));
  strlcpy((char *)req.originalUrl, originalUrl, originalUrlLen + 1);
  req.url = (char *)req.originalUrl;

  char *path = (char *)req.originalUrl;
  char *queryStringStart = strchr(path, '?');

  if (queryStringStart)
  {
    size_t queryStringLen = strlen(queryStringStart + 1);
    req.queryString = malloc(sizeof(char) * (queryStringLen + 1));
    strlcpy((char *)req.queryString, queryStringStart + 1, queryStringLen + 1);
    *queryStringStart = '\0';
    req.queryKeyValueCount = 0;
    parseQueryString(req.queryString, req.queryString + queryStringLen, req.queryKeyValues, &req.queryKeyValueCount,
                     sizeof(req.queryKeyValues) / sizeof(req.queryKeyValues[0]));
  }

  size_t pathLen = strlen(path) + 1;
  req.path = malloc(sizeof(char) * pathLen);
  snprintf((char *)req.path, pathLen, "%s", path);

  req.params = reqParamsFactory(&req, baseRouter);
  req.query = reqQueryFactory(&req);
  req.body = reqBodyFactory(&req);
  req.session = reqSessionFactory(&req);
  req.cookie = reqCookieFactory(&req);
  req.m = reqMiddlewareFactory(&req);
  req.mSet = reqMiddlewareSetFactory(&req);

  req.hostname = req.get("Host");
  req.ip = client.ip;
  req.protocol = "http"; // TODO: TLS/SSL support
  req.secure = strcmp(req.protocol, "https") == 0;
  char *XRequestedWith = req.get("X-Requested-With");
  req.xhr = XRequestedWith && strcmp(XRequestedWith, "XMLHttpRequest") == 0;

  req.middlewareStackCount = 0;

  req._method = req.body("_method");
  if (req._method)
  {
    toUpper((char *)req._method);
    if (strcmp(req._method, "PUT") == 0 || strcmp(req._method, "DELETE") == 0 || strcmp(req._method, "PATCH") == 0)
    {
      free((void *)req.method);
      req.method = req._method;
    }
  }

  return req;
}

static route_handler_t matchRouteHandler(request_t *req, router_t *router)
{
  for (int i = 0; i < router->routeHandlerCount; i++)
  {
    size_t methodLen = strlen(router->routeHandlers[i].method);
    size_t pathLen = strlen(router->routeHandlers[i].path);
    size_t basePathLen = strlen(router->basePath);

    char *routeHandlerFullPath;
    if (basePathLen && pathLen == 1 && router->routeHandlers[i].path[0] == '/')
    {
      routeHandlerFullPath = malloc(sizeof(char) * (basePathLen + 1));
      strlcpy(routeHandlerFullPath, router->basePath, basePathLen + 1);
    }
    else
    {
      routeHandlerFullPath = malloc(sizeof(char) * (basePathLen + pathLen + 1));
      snprintf(routeHandlerFullPath, basePathLen + pathLen + 1, "%s%s", router->basePath, router->routeHandlers[i].path);
    }

    if (strncmp(router->routeHandlers[i].method, req->method, methodLen) != 0)
    {
      free(routeHandlerFullPath);
      continue;
    }

    if (strcmp(routeHandlerFullPath, req->pathMatch) == 0)
    {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    if (strcmp(routeHandlerFullPath, req->path) == 0)
    {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    free(routeHandlerFullPath);
  }
  return (route_handler_t){.method = NULL, .path = NULL, .handler = NULL};
}

static void freeRequest(request_t req)
{
  cleanupHandler *middlewareCleanupBlocks = (void (^*)(request_t *))req.middlewareCleanupBlocks;
  for (int i = 0; i < req.middlewareStackCount; i++)
  {
    middlewareCleanupBlocks[i](&req);
  }
  free(req.middlewareCleanupBlocks);
  if (req._method != NULL)
    curl_free((void *)req._method);
  else
    free((void *)req.method);
  free((void *)req.path);
  free((void *)req.url);
  free(req.session);
  if (req.paramMatch != NULL)
  {
    for (int i = 0; i < req.paramMatch->count; i++)
    {
      free(req.paramMatch->keys[i]);
    }
  }
  if (strlen(req.pathMatch) > 0)
    free((void *)req.pathMatch);
  free(req.paramMatch);
  free((void *)req.hostname);
  free((void *)req.cookiesString);
  free((void *)req.rawRequestBody);
  if (strlen(req.queryString) > 0)
    free((void *)req.queryString);
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
  Block_release(req.get);
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
  Block_release(res.sendStatus);
  Block_release(res.set);
  Block_release(res.get);
  Block_release(res.cookie);
  Block_release(res.location);
  Block_release(res.redirect);
  Block_release(res.download);
  Block_release(res.type);
  Block_release(res.json);
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
  res.send = resSendFactory(client, &res);
  res.sendf = resSendfFactory(&res);
  res.sendFile = resSendFileFactory(client, req, &res);
  res.sendStatus = resSendStatusFactory(&res);
  res.set = resSetFactory(&res);
  res.get = resGetFactory(&res);
  res.cookie = resCookieFactory(&res);
  res.location = resLocationFactory(req, &res);
  res.redirect = resRedirectFactory(req, &res);
  res.type = resTypeFactory(&res);
  res.json = resJsonFactory(&res);
  res.download = resDownloadFactory(&res);
  res.didSend = 0;
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
  int serverSocket;
  int epollFd;
  router_t *baseRouter;
} client_thread_args_t;

void *clientAcceptEventHandler(void *args)
{
  client_thread_args_t *clientThreadArgs = (client_thread_args_t *)args;

  int epollFd = clientThreadArgs->epollFd;
  int serverSocket = clientThreadArgs->serverSocket;
  router_t *baseRouter = clientThreadArgs->baseRouter;

  struct epoll_event *events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
  struct epoll_event ev;
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
      if (events[n].data.fd == serverSocket)
      {
        while (1)
        {
          client_t client = acceptClientConnection(serverSocket);
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

          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

          http_status_t *status = malloc(sizeof(http_status_t));
          status->client = client;
          status->reqStatus = READING;

          ev.data.ptr = status;

          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, client.socket, &ev) < 0)
          {
            // perror("epoll_ctl() failed");
            free(status);
            continue;
          }
        }
      }
      else
      {
        http_status_t *status = (http_status_t *)events[n].data.ptr;
        if (status->reqStatus == READING)
        {
          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
          ev.data.ptr = status;

          client_t client = status->client;

          request_t req = buildRequest(client, baseRouter);

          if (req.method == NULL)
          {
            closeClientConnection(client);
            freeRequest(req);
            continue;
          }

          __block response_t res = buildResponse(client, &req);

          baseRouter->handler(&req, &res);

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

static int initClientAcceptEventHandler(int serverSocket, UNUSED dispatch_queue_t serverQueue, router_t *baseRouter)
{

  pthread_t threads[THREAD_NUM];

  int epollFd = epoll_create1(0);
  if (epollFd < 0)
  {
    fprintf(stderr, "error while creating epoll fd\n");
    return -1;
  }

  client_thread_args_t threadArgs;
  threadArgs.epollFd = epollFd;
  threadArgs.serverSocket = serverSocket;
  threadArgs.baseRouter = baseRouter;

  struct epoll_event epollEvent;
  epollEvent.events = EPOLLIN | EPOLLET;
  epollEvent.data.fd = serverSocket;

  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSocket, &epollEvent) < 0)
  {
    fprintf(stderr, "error while adding listen fd to epoll inst\n");
    return -1;
  }

  for (int i = 0; i < THREAD_NUM; ++i)
  {
    if (pthread_create(&threads[i], NULL, clientAcceptEventHandler, &threadArgs) < 0)
    {
      fprintf(stderr, "error while creating %d thread\n", i);
      return -1;
    }
  }
  clientAcceptEventHandler(&threadArgs);

  return 0;
}
/*

An synchronous, single-threaded implementation of the client handler that uses select().

Keeping around for reference.

*/
UNUSED static void initClientAcceptEventHandlerSelect(int serverSocket, UNUSED dispatch_queue_t serverQueue, router_t *baseRouter)
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

  while (1)
  {
    FD_ZERO(&readFdSet);
    FD_SET(serverSocket, &readFdSet);
    int maxSocket = serverSocket;

    for (int i = 0; i < maxClients; i++)
    {
      if (clients[i].socket > 0)
        FD_SET(clients[i].socket, &readFdSet);

      if (clients[i].socket > maxSocket)
        maxSocket = clients[i].socket;
    }

    activity = select(maxSocket + 1, &readFdSet, NULL, NULL, &waitd);

    if (FD_ISSET(serverSocket, &readFdSet))
    {
      client_t client = acceptClientConnection(serverSocket);
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
        request_t req = buildRequest(client, baseRouter);

        if (req.method == NULL)
        {
          closeClientConnection(client);
          freeRequest(req);
          return;
        }

        __block response_t res = buildResponse(client, &req);

        baseRouter->handler(&req, &res);

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
static void initClientAcceptEventHandler(int serverSocket, dispatch_queue_t serverQueue, router_t *baseRouter)
{
  dispatch_source_t acceptSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, serverSocket, 0, serverQueue);

  dispatch_source_set_event_handler(acceptSource, ^{
    const unsigned long numPendingConnections = dispatch_source_get_data(acceptSource);
    for (unsigned long i = 0; i < numPendingConnections; i++)
    {
      client_t client = acceptClientConnection(serverSocket);
      if (client.socket < 0)
        continue;

      dispatch_source_t readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, client.socket, 0, serverQueue);
      dispatch_source_set_event_handler(readSource, ^{
        request_t req = buildRequest(client, baseRouter);

        if (req.method == NULL)
        {
          closeClientConnection(client);
          freeRequest(req);
          dispatch_source_cancel(readSource);
          dispatch_release(readSource);
          return;
        }

        __block response_t res = buildResponse(client, &req);

        baseRouter->handler(&req, &res);

        closeClientConnection(client);
        freeRequest(req);
        freeResponse(res);
        dispatch_source_cancel(readSource);
        dispatch_release(readSource);
      });
      dispatch_resume(readSource);
    }
  });
  dispatch_resume(acceptSource);
}
#endif

/* Public functions */

char *matchEmbeddedFile(const char *path, embedded_files_data_t embeddedFiles)
{
  size_t pathLen = strlen(path);
  for (int i = 0; i < embeddedFiles.count; i++)
  {
    int match = 1;
    size_t nameLen = strlen(embeddedFiles.names[i]);
    if (pathLen != nameLen)
    {
      continue;
    }
    for (size_t j = 0; j < nameLen; j++)
    {
      if (embeddedFiles.names[i][j] != path[j] && embeddedFiles.names[i][j] != '_')
      {
        match = 0;
      }
    }
    if (match)
    {
      char *data = malloc(sizeof(char) * (embeddedFiles.lengths[i] + 1));
      memcpy(data, embeddedFiles.data[i], embeddedFiles.lengths[i]);
      data[embeddedFiles.lengths[i]] = '\0';
      return data;
    }
  }
  return (char *)NULL;
};

char *cwdFullPath(const char *path)
{
  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  size_t fullPathLen = strlen(cwd) + strlen(path) + 2;
  char *fullPath = malloc(sizeof(char) * fullPathLen);
  snprintf(fullPath, fullPathLen, "%s/%s", cwd, path);
  return fullPath;
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

/* Public middleware */

middlewareHandler expressStatic(const char *path, const char *fullPath, embedded_files_data_t embeddedFiles)
{
  return Block_copy(^(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    if (embeddedFiles.count > 0)
    {
      const char *reqPath = req->path;
      reqPath++;
      char *data = matchEmbeddedFile(reqPath, embeddedFiles);
      const char *mimetype = getMegaMimeType(req->path);
      res->set("Content-Type", mimetype);
      if (data != NULL)
      {
        res->send(data);
        free(data);
      }
      else
      {
        next();
      }
      return;
    }

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

    if (hash_has(memSessionStore, (char *)req->session->uuid))
    {
      req->session->store = hash_get(memSessionStore, (char *)req->session->uuid);
    }
    else
    {
      req->session->store = hash_new();
      dispatch_sync(memSessionQueue, ^{
        hash_set(memSessionStore, (char *)req->session->uuid, req->session->store);
      });
    }

    req->session->get = ^(const char *key) {
      return hash_get(req->session->store, (char *)key);
    };

    req->session->set = ^(const char *key, void *value) {
      hash_set(req->session->store, (char *)key, value);
    };

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    next();
  });
}

/* expressRouter */

router_t *expressRouter(char *basePath)
{
  __block router_t *router = malloc(sizeof(router_t));
  router->basePath = basePath;
  router->routeHandlers = malloc(sizeof(route_handler_t));
  router->routeHandlerCount = 0;
  router->middlewares = malloc(sizeof(middleware_t));
  router->middlewareCount = 0;
  router->routers = malloc(sizeof(router_t));
  router->routerCount = 0;

  void (^addRouteHandler)(const char *, const char *, requestHandler) = ^(const char *method, const char *path, requestHandler handler) {
    int regex = strchr(path, ':') != NULL;
    router->routeHandlers = realloc(router->routeHandlers, sizeof(route_handler_t) * (router->routeHandlerCount + 1));
    route_handler_t routeHandler = {
        .method = method,
        .path = path,
        .regex = regex,
        .handler = handler,
    };
    if (strlen(router->basePath) == 0)
    {
      routeHandler.basePath = (char *)router->basePath;
      routeHandler.paramMatch = regex ? paramMatch(router->basePath, path) : NULL;
    }
    router->routeHandlers[router->routeHandlerCount++] = routeHandler;
  };

  router->get = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("GET", path, handler);
  });

  router->post = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("POST", path, handler);
  });

  router->put = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("PUT", path, handler);
  });

  router->patch = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("PATCH", path, handler);
  });

  router->delete = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("DELETE", path, handler);
  });

  router->use = Block_copy(^(middlewareHandler handler) {
    router->middlewares = realloc(router->middlewares, sizeof(middleware_t) * (router->middlewareCount + 1));
    router->middlewares[router->middlewareCount++] = (middleware_t){.handler = handler};
  });

  router->useRouter = Block_copy(^(router_t *_router) {
    char *_basePath = (char *)_router->basePath;
    size_t basePathLen = strlen(router->basePath) + strlen(_basePath) + 1;
    _router->basePath = malloc(basePathLen);
    snprintf((char *)_router->basePath, basePathLen, "%s%s", router->basePath, _basePath);
    router->routers = realloc(router->routers, sizeof(router_t) * (router->routerCount + 1));
    router->routers[router->routerCount++] = _router;

    for (int i = 0; i < _router->routeHandlerCount; i++)
    {
      _router->routeHandlers[i].basePath = (char *)_router->basePath;
      if (_router->routeHandlers[i].regex)
      {
        _router->routeHandlers[i].paramMatch = paramMatch(_router->basePath, _router->routeHandlers[i].path);
      }
    }
  });

  router->handler = Block_copy(^(UNUSED request_t *req, UNUSED response_t *res) {
    runMiddleware(0, req, res, router, ^{
      route_handler_t routeHandler = matchRouteHandler(req, router);
      if (routeHandler.handler != NULL)
      {
        req->baseUrl = routeHandler.basePath;
        req->route = (void *)&routeHandler;
        routeHandler.handler(req, res);
        return;
      }
    });

    for (int i = 0; i < router->routerCount; i++)
    {
      router->routers[i]->handler(req, res);
    }

    if (res->didSend == 0)
    {
      res->status = 404;
      res->sendf(errorHTML, req->path);
    }
  });

  return router;
}

/* express */

app_t express()
{
  __block int appCleanupCount = 0;
  __block appCleanupHandler *appCleanupBlocks = malloc(sizeof(appCleanupHandler));
  __block int serverSocket = -1;
  __block dispatch_queue_t serverQueue = dispatch_queue_create("serverQueue", DISPATCH_QUEUE_CONCURRENT);

  initServerSocket(&serverSocket);

  app_t app;
  __block router_t *baseRouter = expressRouter("");

  app.get = Block_copy(^(const char *path, requestHandler handler) {
    baseRouter->get(path, handler);
  });

  app.post = Block_copy(^(const char *path, requestHandler handler) {
    baseRouter->post(path, handler);
  });

  app.put = Block_copy(^(const char *path, requestHandler handler) {
    baseRouter->put(path, handler);
  });

  app.patch = Block_copy(^(const char *path, requestHandler handler) {
    baseRouter->patch(path, handler);
  });

  app.delete = Block_copy(^(const char *path, requestHandler handler) {
    baseRouter->delete (path, handler);
  });

  app.use = Block_copy(^(middlewareHandler handler) {
    baseRouter->use(handler);
  });

  app.useRouter = Block_copy(^(router_t *router) {
    baseRouter->useRouter(router);
  });

  app.cleanup = Block_copy(^(appCleanupHandler handler) {
    appCleanupBlocks = realloc(appCleanupBlocks, sizeof(appCleanupHandler) * (appCleanupCount + 1));
    appCleanupBlocks[appCleanupCount++] = handler;
  });

  app.closeServer = Block_copy(^() {
    printf("\nClosing server...\n");
    freeRouteHandlers(baseRouter->routeHandlers, baseRouter->routeHandlerCount);
    freeMiddlewares(baseRouter->middlewares, baseRouter->middlewareCount);
    close(serverSocket);
    dispatch_release(serverQueue);
    for (int i = 0; i < appCleanupCount; i++)
    {
      appCleanupBlocks[i]();
    }
  });

  app.listen = Block_copy(^(int port, void (^callback)()) {
    initServerListen(port, &serverSocket);
    dispatch_async(serverQueue, ^{
      initClientAcceptEventHandler(serverSocket, serverQueue, baseRouter);
    });
    callback();
    dispatch_main();
  });

  return app;
};
