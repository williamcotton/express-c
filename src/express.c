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

#include "express.h"
#include "status_message.h"
#include <Block.h>
#include <MegaMimes/MegaMimes.h>
#include <arpa/inet.h>
#include <assert.h>
#include <curl/curl.h>
#include <dispatch/dispatch.h>
#include <picohttpparser/picohttpparser.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

#ifdef __linux__
#include <bsd/string.h>
#include <pthread.h>
#include <sys/epoll.h>
#endif

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

static void removeWhitespace(char *str) {
  char *p = str;
  char *q = str;
  while (*p) {
    if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
      *q++ = *p;
    }
    p++;
  }
  *q = '\0';
}

static const char **split(char *str, char *delim, int *count) {
  char **result = NULL;
  char *token = NULL;
  int i = 0;

  token = strtok(str, delim);
  while (token != NULL) {
    result = realloc(result, sizeof(char *) * (i + 1));
    removeWhitespace(token);
    result[i] = token;
    i++;
    token = strtok(NULL, delim);
  }

  *count = i;
  return (const char **)result;
}

static size_t getFileSize(const char *filePath) {
  struct stat st;
  check(stat(filePath, &st) >= 0, "Could not stat file %s", filePath);
  return st.st_size;
error:
  return 0;
}

static char *getFileName(const char *filePath) {
  char *fileName = strrchr(filePath, '/');
  if (fileName)
    return fileName + 1;
  return (char *)filePath;
}

static void toUpper(char *givenStr) {
  int i;
  for (i = 0; givenStr[i] != '\0'; i++) {
    if (givenStr[i] >= 'a' && givenStr[i] <= 'z') {
      givenStr[i] = givenStr[i] - 32;
    }
  }
}

static void parseQueryString(const char *buf, const char *bufEnd,
                             key_value_t *keyValues, size_t *keyValueCount,
                             size_t max) {
  const char *keyStart = buf;
  const char *keyEnd = NULL;
  const char *valueStart = NULL;
  const char *valueEnd = NULL;
  size_t keyLen = 0;
  size_t valueLen = 0;
  while (buf <= bufEnd) {
    if (*buf == '=') {
      keyEnd = buf;
      keyLen = keyEnd - keyStart;
      valueStart = buf + 1;
    } else if (*buf == '&' || *buf == '\0') {
      valueEnd = buf;
      valueLen = valueEnd - valueStart;
      if (*keyValueCount < max) {
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

typedef struct client_t {
  int socket;
  char *ip;
} client_t;

static param_match_t *paramMatch(const char *basePath, const char *route) {
  param_match_t *pm = malloc(sizeof(param_match_t));
  pm->keys = malloc(sizeof(char *));
  pm->count = 0;
  pm->regexRoute = NULL;
  char regexRoute[4096];
  regexRoute[0] = '\0';
  size_t basePathRouteLen = strlen(basePath) + strlen(route) + 1;
  char *basePathRoute = malloc(basePathRouteLen);
  snprintf(basePathRoute, basePathRouteLen, "%s%s", basePath, route);
  const char *source = basePathRoute;
  char *regexString = ":([A-Za-z0-9_]*)";
  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;
  char *cursor;

  if (regcomp(&regexCompiled, regexString, REG_EXTENDED)) {
    log_err("regcomp() failed");
    free(basePathRoute);
    return pm;
  };

  cursor = (char *)source;
  for (m = 0; m < maxMatches; m++) {
    if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))
      break; // No more matches

    unsigned int g = 0;
    unsigned int offset = 0;
    for (g = 0; g < maxGroups; g++) {
      if (groupArray[g].rm_so == (long long)(size_t)-1)
        break; // No more groups

      char cursorCopy[strlen(cursor) + 1];
      strlcpy(cursorCopy, cursor, groupArray[g].rm_eo + 1);

      if (g == 0) {
        offset = groupArray[g].rm_eo;
        sprintf(regexRoute + strlen(regexRoute), "%.*s([^\\/\\s]*)",
                (int)groupArray[g].rm_so, cursorCopy);
      } else {
        pm->keys = realloc(pm->keys, sizeof(char *) * (m + 1));
        pm->count++;
        char *key = malloc(sizeof(char) *
                           (groupArray[g].rm_eo - groupArray[g].rm_so + 1));
        strlcpy(key, cursorCopy + groupArray[g].rm_so,
                groupArray[g].rm_eo - groupArray[g].rm_so + 1);
        pm->keys[m] = key;
      }
    }
    cursor += offset;
  }

  sprintf(regexRoute + strlen(regexRoute), "%s", cursor);

  regfree(&regexCompiled);

  size_t regesRouteLen = strlen(regexRoute) + 3;
  pm->regexRoute = malloc(sizeof(char) * (regesRouteLen));
  snprintf(pm->regexRoute, regesRouteLen, "^%s$", regexRoute);

  free(basePathRoute);

  return pm;
}

typedef void * (^mallocBlock)(size_t);
static mallocBlock reqMallocFactory(request_t *req) {
  req->mallocCount = 0;
  return Block_copy(^(size_t size) {
    void *ptr = malloc(size);
    req->mallocs[req->mallocCount++] = (req_malloc_t){.ptr = ptr};
    return ptr;
  });
}

typedef void * (^copyBlock)(void *);
static copyBlock reqBlockCopyFactory(request_t *req) {
  req->blockCopyCount = 0;
  return Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    req->blockCopies[req->blockCopyCount++] = (req_block_copy_t){.ptr = ptr};
    return ptr;
  });
}

typedef char * (^getBlock)(const char *key);
static getBlock reqQueryFactory(request_t *req) {
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i != req->queryKeyValueCount; ++i) {
      size_t keyLen = strlen(key);
      char *decodedKey =
          curl_easy_unescape(req->curl, req->queryKeyValues[i].key,
                             req->queryKeyValues[i].keyLen, NULL);
      if (strncmp(decodedKey, key, keyLen) == 0) {
        curl_free(decodedKey);
        char *value =
            req->malloc(sizeof(char) * (req->queryKeyValues[i].valueLen + 1));
        strlcpy(value, req->queryKeyValues[i].value,
                req->queryKeyValues[i].valueLen + 1);
        char *decodedCurlValue = curl_easy_unescape(
            req->curl, value, req->queryKeyValues[i].valueLen, NULL);
        char *decodedValue =
            req->malloc(sizeof(char) * strlen(decodedCurlValue) + 1);
        strncpy(decodedValue, decodedCurlValue, strlen(decodedCurlValue) + 1);
        curl_free(decodedCurlValue);
        return decodedValue;
      }
      curl_free(decodedKey);
    }
    return (char *)NULL;
  });
}

static session_t *reqSessionFactory(UNUSED request_t *req) {
  session_t *session = malloc(sizeof(session_t));
  return session;
}

static void routeMatch(const char *path, const char *regexRoute,
                       key_value_t *paramKeyValues, int *match) {
  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;

  if (regcomp(&regexCompiled, regexRoute, REG_EXTENDED)) {
    log_err("regcomp() failed");
    return;
  };

  const char *cursor = path;
  for (m = 0; m < maxMatches; m++) {
    if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))
      break; // No more matches

    unsigned int g = 0;
    unsigned int offset = 0;
    for (g = 0; g < maxGroups; g++) {
      if (groupArray[g].rm_so == (long long)(size_t)-1)
        break; // No more groups

      if (g == 0) {
        offset = groupArray[g].rm_eo;
        *match = 1;
      } else {
        int index = g - 1;
        paramKeyValues[index].value = cursor + groupArray[g].rm_so;
        paramKeyValues[index].valueLen =
            groupArray[g].rm_eo - groupArray[g].rm_so;
      }
    }
    cursor += offset;
  }

  regfree(&regexCompiled);
}

static void collectRegexRouteHandlers(router_t *router,
                                      route_handler_t *regExRouteHandlers,
                                      int *regExRouteHandlerCount) {
  for (int i = 0; i < router->routeHandlerCount; i++) {
    if (router->routeHandlers[i].paramMatch != NULL) {
      regExRouteHandlers[*regExRouteHandlerCount] = router->routeHandlers[i];
      (*regExRouteHandlerCount)++;
    }
  }

  for (int i = 0; i < router->routerCount; ++i)
    collectRegexRouteHandlers(router->routers[i], regExRouteHandlers,
                              regExRouteHandlerCount);
}

static getBlock reqGetFactory(request_t *req) {
  return Block_copy(^(const char *headerKey) {
    for (size_t i = 0; i != req->numHeaders; ++i) {
      if (strncmp(req->headers[i].name, headerKey, req->headers[i].name_len) ==
          0) {
        char *value =
            req->malloc(sizeof(char) * (req->headers[i].value_len + 1));
        sprintf(value, "%.*s", (int)req->headers[i].value_len,
                req->headers[i].value);
        return value;
      }
    }
    return (char *)NULL;
  });
}

static getBlock reqParamsFactory(request_t *req, router_t *baseRouter) {
  route_handler_t regExRouteHandlers[4096];
  int regExRouteHandlerCount = 0;
  collectRegexRouteHandlers(baseRouter, regExRouteHandlers,
                            &regExRouteHandlerCount);
  req->pathMatch = "";
  for (int i = 0; i < regExRouteHandlerCount; i++) {
    int match = 0;

    routeMatch(req->path, regExRouteHandlers[i].paramMatch->regexRoute,
               req->paramKeyValues, &match);
    if (match) {
      int pathMatchLen = strlen(regExRouteHandlers[i].basePath) +
                         strlen(regExRouteHandlers[i].path) + 1;
      req->pathMatch = req->malloc(sizeof(char) * pathMatchLen);
      snprintf((char *)req->pathMatch, pathMatchLen, "%s%s",
               regExRouteHandlers[i].basePath, regExRouteHandlers[i].path);
      req->paramKeyValueCount = regExRouteHandlers[i].paramMatch->count;
      for (int j = 0; j < regExRouteHandlers[i].paramMatch->count; j++) {
        req->paramKeyValues[j].key = regExRouteHandlers[i].paramMatch->keys[j];
        req->paramKeyValues[j].keyLen =
            strlen(regExRouteHandlers[i].paramMatch->keys[j]);
      }
      break;
    }
  }
  return Block_copy(^(const char *key) {
    for (size_t j = 0; j < req->paramKeyValueCount; j++) {
      size_t keyLen = strlen(key);
      if (strncmp(req->paramKeyValues[j].key, key, keyLen) == 0) {
        char *value =
            req->malloc(sizeof(char) * (req->paramKeyValues[j].valueLen + 1));
        strlcpy(value, req->paramKeyValues[j].value,
                req->paramKeyValues[j].valueLen + 1);
        return (char *)value;
      }
    }
    return (char *)NULL;
  });
}

static getBlock reqCookieFactory(request_t *req) {
  req->cookiesKeyValueCount = 0;
  req->cookiesString = (char *)req->get("Cookie");
  memset(req->cookies, 0, sizeof(req->cookies));
  char *cookies = (char *)req->cookiesString;
  if (req->cookiesString != NULL) {
    char *cookie = strtok(cookies, ";");
    int i = 0;
    while (cookie != NULL) {
      req->cookies[i] = cookie;
      cookie = strtok(NULL, ";");
      req->cookiesKeyValueCount = ++i;
    }
    for (i = 0; i < 4096; i++) {
      if (req->cookies[i] == NULL)
        break;
      char *key = strtok((char *)req->cookies[i], "=");
      char *value = strtok(NULL, "=");
      if (key[0] == ' ')
        key++;
      req->cookiesKeyValues[i].key = key;
      req->cookiesKeyValues[i].keyLen = strlen(key);
      req->cookiesKeyValues[i].value = value;
      req->cookiesKeyValues[i].valueLen = strlen(value);
    }
  }

  return Block_copy(^(const char *key) {
    check_silent(req->cookiesKeyValueCount > 0, "No cookies found");
    for (size_t j = req->cookiesKeyValueCount - 1; j >= 0; j--) {
      check_silent(req->cookiesKeyValues[j].key != NULL, "No cookies found");
      size_t keyLen = strlen(key);
      if (strncmp(req->cookiesKeyValues[j].key, key, keyLen) == 0) {
        char *value =
            req->malloc(sizeof(char) * (req->cookiesKeyValues[j].valueLen + 1));
        strlcpy(value, req->cookiesKeyValues[j].value,
                req->cookiesKeyValues[j].valueLen + 1);
        return (char *)value;
      }
    }
  error:
    return (char *)NULL;
  });
}

typedef void * (^getMiddlewareBlock)(const char *key);
static getMiddlewareBlock reqMiddlewareFactory(request_t *req) {
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i < req->middlewareKeyValueCount; i++) {
      if (strcmp(req->middlewareKeyValues[i].key, key) == 0)
        return req->middlewareKeyValues[i].value;
    }
    return NULL;
  });
}

typedef void (^getMiddlewareSetBlock)(const char *key, void *middleware);
static getMiddlewareSetBlock reqMiddlewareSetFactory(request_t *req) {
  req->middlewareKeyValueCount = 0;
  return Block_copy(^(const char *key, void *middleware) {
    req->middlewareKeyValues[req->middlewareKeyValueCount].key = key;
    req->middlewareKeyValues[req->middlewareKeyValueCount].value = middleware;
    req->middlewareKeyValueCount++;
  });
}

static getBlock reqBodyFactory(request_t *req) {
  req->bodyKeyValueCount = 0;
  if (strncmp(req->method, "POST", 4) == 0 ||
      strncmp(req->method, "PATCH", 5) == 0 ||
      strncmp(req->method, "PUT", 3) == 0) {
    char *rawRequest = (char *)req->rawRequest;
    char *body = strstr(rawRequest, "\r\n\r\n");
    body += 4;

    req->bodyString = req->malloc(sizeof(char) * req->contentLength + 1);
    memcpy((char *)req->bodyString, body, req->contentLength);
    req->bodyString[req->contentLength] = '\0';
    if (req->bodyString && strlen(req->bodyString) > 0) {
      char *contentType = (char *)req->get("Content-Type");
      if (strncmp(contentType, "application/x-www-form-urlencoded", 33) == 0) {
        size_t bodyStringLen = strlen(req->bodyString);
        parseQueryString(req->bodyString, req->bodyString + bodyStringLen,
                         req->bodyKeyValues, &req->bodyKeyValueCount,
                         sizeof(req->bodyKeyValues) /
                             sizeof(req->bodyKeyValues[0]));
      } else if (strncmp(contentType, "application/json", 16) == 0) {
        // printf("application/json: %s\n", req->bodyString);
      } else if (strncmp(contentType, "multipart/form-data", 20) == 0) {
        // printf("multipart/form-data: %s\n", req->bodyString);
      }
    } else {
      req->bodyString[0] = '\0';
    }
  }
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i != req->bodyKeyValueCount; ++i) {
      size_t keyLen = strlen(key);
      char *decodedKey =
          curl_easy_unescape(req->curl, req->bodyKeyValues[i].key,
                             req->bodyKeyValues[i].keyLen, NULL);
      if (strncmp(decodedKey, key, keyLen) == 0) {
        char *value =
            req->malloc(sizeof(char) * (req->bodyKeyValues[i].valueLen + 1));
        strlcpy(value, req->bodyKeyValues[i].value,
                req->bodyKeyValues[i].valueLen + 1);
        int j = 0;
        while (value[j] != '\0') {
          if (value[j] == '+')
            value[j] = ' ';
          j++;
        }
        char *decodedCurlValue = curl_easy_unescape(
            req->curl, value, req->bodyKeyValues[i].valueLen, NULL);
        char *decodedValue =
            req->malloc(sizeof(char) * strlen(decodedCurlValue) + 1);
        strncpy(decodedValue, decodedCurlValue, strlen(decodedCurlValue) + 1);
        curl_free(decodedCurlValue);
        curl_free(decodedKey);
        return decodedValue;
      }
      curl_free(decodedKey);
    }
    return (char *)NULL;
  });
}

static char *buildResponseString(const char *body, response_t *res) {
  if (res->get("Content-Type") == NULL)
    res->set("Content-Type", "text/html; charset=utf-8");

  char *contentLength = malloc(sizeof(char) * 20);
  sprintf(contentLength, "%zu", strlen(body));
  res->set("Content-Length", contentLength);

  res->set("Connection", "close");

  char *statusMessage = getStatusMessage(res->status);
  char *status = malloc(sizeof(char) * (strlen(statusMessage) + 5));
  sprintf(status, "%d %s", res->status, statusMessage);

  size_t headersLength = 0;
  char headers[4096];
  memset(headers, 0, 4096);

  for (size_t i = 0; i < res->headersKeyValueCount; i++) {
    size_t headerLength =
        res->headersKeyValues[i].keyLen + res->headersKeyValues[i].valueLen + 4;
    if (headersLength + headerLength > 4096)
      break;
    sprintf(headers + headersLength, "%s: %s\r\n", res->headersKeyValues[i].key,
            res->headersKeyValues[i].value);
    headersLength += headerLength;
  }

  char *allHeaders =
      malloc(sizeof(char) * (strlen("HTTP/1.1 \r\n\r\n") + strlen(status) +
                             headersLength + res->cookieHeadersLength + 1));
  sprintf(allHeaders, "HTTP/1.1 %s\r\n%s%s\r\n", status, headers,
          res->cookieHeaders);

  size_t allHeadersLen = strlen(allHeaders) + 1;
  size_t bodyLen = strlen(body);
  char *responseString = malloc(sizeof(char) * (allHeadersLen + bodyLen));
  strncpy(responseString, allHeaders, allHeadersLen);
  strncat(responseString, body, bodyLen);
  free(status);
  free(allHeaders);
  free(contentLength);
  return responseString;
}

typedef void (^sendBlock)(const char *body);
static sendBlock resSendFactory(client_t client, response_t *res) {
  return Block_copy(^(const char *body) {
    if (res->didSend == 1)
      return;
    char *response = buildResponseString(body, res);
    res->didSend = 1;
    write(client.socket, response, strlen(response));
    free(response);
  });
}

typedef void (^sendfBlock)(const char *format, ...);
static sendfBlock resSendfFactory(response_t *res) {
  return Block_copy(^(const char *format, ...) {
    char body[65536];
    va_list args;
    va_start(args, format);
    vsnprintf(body, 65536, format, args);
    res->send(body);
    va_end(args);
  });
}

static sendBlock resSendFileFactory(client_t client, request_t *req,
                                    response_t *res) {
  return Block_copy(^(const char *path) {
    if (res->didSend == 1)
      return;
    FILE *file = fopen(path, "r");
    if (file == NULL) {
      res->status = 404;
      res->sendf(errorHTML, req->path);
      return;
    }
    char *mimetype =
        (char *)getMegaMimeType((const char *)path); // TODO: check for NULL
    char *response =
        malloc(sizeof(char) *
               (strlen(mimetype) +
                strlen("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: "
                       "\r\nContent-Length: \r\n\r\n") +
                20));
    // TODO: use res.set() and refactor header building
    size_t fileSize = getFileSize(path);
    sprintf(response,
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: "
            "%s\r\nContent-Length: %zu\r\n\r\n",
            mimetype, fileSize);
    res->didSend = 1;
    write(client.socket, response, strlen(response));
    // TODO: use sendfile
    char *buffer = malloc(4096);
    size_t bytesRead = fread(buffer, 1, 4096, file);
    while (bytesRead > 0) {
      write(client.socket, buffer, bytesRead);
      bytesRead = fread(buffer, 1, 4096, file);
    }
    free(buffer);
    free(response);
    fclose(file);
  });
}

typedef void (^sendStatusBlock)(int status);
static sendStatusBlock resSendStatusFactory(response_t *res) {
  return Block_copy(^(int status) {
    res->status = status;
    res->send(getStatusMessage(status));
  });
}

typedef void (^typeBlock)(const char *type);
static typeBlock resTypeFactory(response_t *res) {
  return Block_copy(^(const char *type) {
    const char *mimetype = getMegaMimeType(type);
    if (mimetype != NULL)
      res->set("Content-Type", mimetype);
  });
}

typedef void (^jsonBlock)(const char *json);
static jsonBlock resJsonFactory(response_t *res) {
  return Block_copy(^(const char *json) {
    res->set("Content-Type", "application/json");
    res->send(json);
  });
}

typedef void (^downloadBlock)(const char *filePath, const char *name);
static downloadBlock resDownloadFactory(request_t *req, response_t *res) {
  return Block_copy(^(const char *filePath, const char *fileName) {
    if (fileName == NULL)
      fileName = (char *)getFileName(filePath);

    char *contentDisposition = req->malloc(
        sizeof(char) *
        (strlen("Content-Disposition: attachment; filename=\"\"\r\n") +
         strlen(fileName) + 1));
    sprintf(contentDisposition,
            "Content-Disposition: attachment; filename=\"%s\"\r\n", fileName);
    res->set("Content-Disposition", contentDisposition);
    res->sendFile(filePath);
  });
}

typedef void (^setBlock)(const char *key, const char *value);
static setBlock resSetFactory(response_t *res) {
  res->headersKeyValueCount = 0;

  return Block_copy(^(const char *key, const char *value) {
    for (size_t i = 0; i < res->headersKeyValueCount; i++) {
      if (strcmp(res->headersKeyValues[i].key, key) == 0) {
        res->headersKeyValues[i].value = value;
        return;
      }
    }

    res->headersKeyValues[res->headersKeyValueCount].key = key;
    res->headersKeyValues[res->headersKeyValueCount].keyLen = strlen(key);
    res->headersKeyValues[res->headersKeyValueCount].value = value;
    res->headersKeyValues[res->headersKeyValueCount].valueLen = strlen(value);
    res->headersKeyValueCount++;
  });
}

static getBlock resGetFactory(response_t *res) {
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i < res->headersKeyValueCount; i++) {
      if (strcmp(res->headersKeyValues[i].key, key) == 0)
        return (char *)res->headersKeyValues[i].value;
    }
    return (char *)NULL;
  });
}

static char *cookieOptsStringFromOpts(cookie_opts_t opts) {
  char cookieOptsString[1024];
  memset(cookieOptsString, 0, 1024);
  size_t i = 0;
  if (opts.httpOnly) {
    strncpy(cookieOptsString + i, "; HttpOnly", 10);
    i += strlen("; HttpOnly");
  }
  if (opts.secure) {
    strncpy(cookieOptsString + i, "; Secure", 8);
    i += strlen("; Secure");
  }
  if (opts.maxAge != 0) {
    size_t maxAgeLen = strlen("; Max-Age=") + 20;
    char *maxAgeValue = malloc(sizeof(char) * maxAgeLen);
    sprintf(maxAgeValue, "; Max-Age=%d", opts.maxAge);
    strncpy(cookieOptsString + i, maxAgeValue, maxAgeLen);
    i += strlen(maxAgeValue);
    free(maxAgeValue);
  }
  if (opts.expires != NULL) {
    size_t expiresLen = strlen("; Expires=") + 20;
    char *expiresValue = malloc(sizeof(char) * expiresLen);
    sprintf(expiresValue, "; Expires=%s", opts.expires);
    strncpy(cookieOptsString + i, expiresValue, expiresLen);
    i += strlen(expiresValue);
    free(expiresValue);
  }
  if (opts.domain != NULL) {
    size_t domainLen = strlen("; Domain=") + strlen(opts.domain) + 1;
    char *domainValue = malloc(sizeof(char) * domainLen);
    sprintf(domainValue, "; Domain=%s", opts.domain);
    strncpy(cookieOptsString + i, domainValue, domainLen);
    i += strlen(domainValue);
    free(domainValue);
  }
  if (opts.path != NULL) {
    size_t pathLen = strlen("; Path=") + strlen(opts.path) + 1;
    char *pathValue = malloc(sizeof(char) * pathLen);
    sprintf(pathValue, "; Path=%s", opts.path);
    strncpy(cookieOptsString + i, pathValue, pathLen);
    free(pathValue);
  }
  return strdup(cookieOptsString);
}

typedef void (^setCookie)(const char *cookieKey, const char *cookieValue,
                          cookie_opts_t opts);
static setCookie resCookieFactory(response_t *res) {
  memset(res->cookieHeaders, 0, sizeof(res->cookieHeaders));
  res->cookieHeadersLength = 0;
  return Block_copy(^(const char *key, const char *value, cookie_opts_t opts) {
    char *cookieOptsString = cookieOptsStringFromOpts(opts);
    size_t valueLen = strlen(value) + 1;
    size_t cookieStringOptsLen = strlen(cookieOptsString);
    char *valueWithOptions =
        malloc(sizeof(char) * (valueLen + cookieStringOptsLen));
    strncpy(valueWithOptions, value, valueLen);
    strncat(valueWithOptions, cookieOptsString, cookieStringOptsLen);

    size_t headersLen = strlen(key) + strlen(valueWithOptions) + 16;
    strncpy(res->cookieHeaders + res->cookieHeadersLength, "Set-Cookie", 10);
    res->cookieHeaders[res->cookieHeadersLength + 10] = ':';
    res->cookieHeaders[res->cookieHeadersLength + 11] = ' ';
    strncpy(res->cookieHeaders + res->cookieHeadersLength + 12, key,
            strlen(key));
    res->cookieHeaders[res->cookieHeadersLength + 12 + strlen(key)] = '=';
    strncpy(res->cookieHeaders + res->cookieHeadersLength + 13 + strlen(key),
            valueWithOptions, strlen(valueWithOptions));
    res->cookieHeaders[res->cookieHeadersLength + 13 + strlen(key) +
                       strlen(valueWithOptions)] = ';';
    res->cookieHeaders[res->cookieHeadersLength + 14 + strlen(key) +
                       strlen(valueWithOptions)] = '\r';
    res->cookieHeaders[res->cookieHeadersLength + 15 + strlen(key) +
                       strlen(valueWithOptions)] = '\n';
    res->cookieHeadersLength += headersLen;

    free(cookieOptsString);
    free(valueWithOptions);
  });
}

typedef void (^urlBlock)(const char *url);
static urlBlock resLocationFactory(request_t *req, response_t *res) {
  return Block_copy(^(const char *url) {
    if (strncmp(url, "back", 4) == 0) {
      const char *referer = req->get("Referer");
      if (referer != NULL)
        res->set("Location", referer);
      else
        res->set("Location", "/");
      return;
    }
    char *location =
        req->malloc(sizeof(char) * (strlen(req->path) + strlen(url) + 2));
    sprintf(location, "%s%s", req->path, url);
    res->set("Location", location);
  });
}

static urlBlock resRedirectFactory(UNUSED request_t *req, response_t *res) {
  return Block_copy(^(const char *url) {
    res->status = 302;
    res->location(url);
    res->sendf("Redirecting to %s", url);
    return;
  });
}

static char *matchFilepath(request_t *req, const char *path) {
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
  if (reti) {
    free(pattern);
    free(buffer);
    log_err("regcomp() failed");
    return NULL;
  }
  reti = regexec(&regex, buffer, nmatch, pmatch, 0);
  if (reti == 0) {
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
  regfree(&regex);
  free(buffer);
  free(pattern);
  return NULL;
}

static void runMiddleware(int index, request_t *req, response_t *res,
                          router_t *router, void (^next)()) {
  if (index < router->middlewareCount) {
    void (^cleanup)(cleanupHandler) = ^(cleanupHandler cleanupBlock) {
      req->middlewareCleanupBlocks = realloc( // NOLINT
          req->middlewareCleanupBlocks,
          sizeof(cleanupHandler *) * (req->middlewareStackCount + 1));
      req->middlewareCleanupBlocks[req->middlewareStackCount++] =
          (void *)cleanupBlock;
    };
    router->middlewares[index].handler(
        req, res,
        ^{
          runMiddleware(index + 1, req, res, router, next);
        },
        cleanup);
  } else {
    next();
  }
}

static client_t acceptClientConnection(server_t *server) {
  int clientSocket = -1;
  struct sockaddr_in echoClntAddr;
  unsigned int clntLen = sizeof(echoClntAddr);

  check_silent(
      (clientSocket = accept(server->socket, (struct sockaddr *)&echoClntAddr,
                             &clntLen)) >= 0,
      "accept() failed");
  check(fcntl(clientSocket, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");

  char *client_ip = inet_ntoa(echoClntAddr.sin_addr);

  return (client_t){.socket = clientSocket, .ip = client_ip};
error:
  shutdown(clientSocket, SHUT_RDWR);
  if (clientSocket >= 0)
    close(clientSocket);
  return (client_t){.socket = -1, .ip = NULL};
}

static int initServerSocket(server_t *server) {
  int flag = 1;
  check((server->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) >= 0,
        "socket() failed");
  check(setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &flag,
                   sizeof(flag)) >= 0,
        "setsockopt() failed");

  return 0;
error:
  return -1;
}

static void paramMatchFree(param_match_t *paramMatch) {
  for (int i = 0; i < paramMatch->count; i++) {
    free(paramMatch->keys[i]);
  }
  free(paramMatch->keys);
  if (paramMatch->regexRoute)
    free(paramMatch->regexRoute);
}

static int initServerListen(int port, server_t *server) {
  // TODO: TLS/SSL support
  struct sockaddr_in servAddr;
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(port);

  check(bind(server->socket, (struct sockaddr *)&servAddr, sizeof(servAddr)) >=
            0,
        "bind() failed");
  check(fcntl(server->socket, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");
  check(listen(server->socket, 10000) >= 0, "listen() failed");

  return 0;
error:
  shutdown(server->socket, SHUT_RDWR);
  close(server->socket);
  return -1;
};

static void buildRequest(request_t *req, client_t client,
                         router_t *baseRouter) {
  memset(req->rawRequest, 0, sizeof(req->rawRequest));
  req->rawRequestSize = 0;

  req->malloc = reqMallocFactory(req);

  char *method, *originalUrl;
  int parseBytes = 0, minorVersion;
  size_t prevBufferLen = 0, methodLen, originalUrlLen;
  ssize_t readBytes;

  time_t start;
  time(&start);
  time_t current;

  while (1) {
    while ((readBytes =
                read(client.socket, req->rawRequest + req->rawRequestSize,
                     sizeof(req->rawRequest) - req->rawRequestSize)) == -1) {
      time(&current);
      time_t difference = difftime(current, start);
      check(difference < READ_TIMEOUT_SECS, "request timeout");
    }
    check_silent(readBytes > 0, "read() failed");
    prevBufferLen = req->rawRequestSize;
    req->rawRequestSize += readBytes;
    req->numHeaders = sizeof(req->headers) / sizeof(req->headers[0]);
    parseBytes = phr_parse_request(
        req->rawRequest, req->rawRequestSize, (const char **)&method,
        &methodLen, (const char **)&originalUrl, &originalUrlLen, &minorVersion,
        req->headers, &req->numHeaders, prevBufferLen);
    if (parseBytes > 0) {
      req->get = reqGetFactory(req);
      char *contentLength = (char *)req->get("Content-Length");
      req->contentLength =
          contentLength != NULL ? strtoll(contentLength, NULL, 10) : 0;
      if (req->contentLength != 0 && parseBytes == readBytes)
        while ((read(client.socket, req->rawRequest + req->rawRequestSize,
                     sizeof(req->rawRequest) - req->rawRequestSize)) == -1)
          ;
      break;
    } else if (parseBytes == -1)
      sentinel("Parse error");
    assert(parseBytes == -2);
    if (req->rawRequestSize == sizeof(req->rawRequest))
      sentinel("Request is too long");
  }

  long long maxBodyLen = (MAX_REQUEST_SIZE)-parseBytes;
  check(req->contentLength <= maxBodyLen, "Request body too large");

  req->blockCopy = reqBlockCopyFactory(req);
  req->middlewareCleanupBlocks = malloc(sizeof(cleanupHandler *)); // NOLINT

  req->curl = curl_easy_init(); // TODO: move to global scope

  req->method = malloc(sizeof(char) * (methodLen + 1));
  strlcpy((char *)req->method, method, methodLen + 1);

  req->originalUrl = malloc(sizeof(char) * (originalUrlLen + 1));
  strlcpy((char *)req->originalUrl, originalUrl, originalUrlLen + 1);
  req->url = (char *)req->originalUrl;

  char *path = (char *)req->originalUrl;
  char *queryStringStart = strchr(path, '?');

  req->queryString = NULL;
  if (queryStringStart) {
    size_t queryStringLen = strlen(queryStringStart + 1);
    req->queryString = malloc(sizeof(char) * (queryStringLen + 1));
    strlcpy((char *)req->queryString, queryStringStart + 1, queryStringLen + 1);
    *queryStringStart = '\0';
    req->queryKeyValueCount = 0;
    parseQueryString(req->queryString, req->queryString + queryStringLen,
                     req->queryKeyValues, &req->queryKeyValueCount,
                     sizeof(req->queryKeyValues) /
                         sizeof(req->queryKeyValues[0]));
  }

  size_t pathLen = strlen(path) + 1;
  req->path = malloc(sizeof(char) * pathLen);
  snprintf((char *)req->path, pathLen, "%s", path);

  req->params = reqParamsFactory(req, baseRouter);
  req->query = reqQueryFactory(req);
  req->body = reqBodyFactory(req);
  req->session = reqSessionFactory(req);
  req->cookie = reqCookieFactory(req);
  req->m = reqMiddlewareFactory(req);
  req->mSet = reqMiddlewareSetFactory(req);

  req->hostname = req->get("Host");
  req->ip = client.ip;
  req->protocol = "http"; // TODO: TLS/SSL support
  req->secure = strcmp(req->protocol, "https") == 0;
  req->XRequestedWith = req->get("X-Requested-With");
  req->xhr =
      req->XRequestedWith && strcmp(req->XRequestedWith, "XMLHttpRequest") == 0;
  req->XForwardedFor = req->get("X-Forwarded-For");
  req->ipsCount = 0;
  req->ips = req->XForwardedFor ? split(req->XForwardedFor, ",", &req->ipsCount)
                                : (const char **)NULL;
  req->subdomainsCount = 0;
  req->subdomains =
      req->hostname ? split((char *)req->hostname, ".", &req->subdomainsCount)
                    : (const char **)NULL;
  if (req->subdomainsCount > 2) {
    req->subdomainsCount -= 2;
  }

  req->middlewareStackCount = 0;

  req->_method = req->body("_method");
  if (req->_method) {
    toUpper((char *)req->_method);
    if (strcmp(req->_method, "PUT") == 0 ||
        strcmp(req->_method, "DELETE") == 0 ||
        strcmp(req->_method, "PATCH") == 0) {
      free((void *)req->method);
      req->method = req->_method;
    }
  }

  return;
error:
  req->method = NULL;
  for (int i = 0; i < req->mallocCount; i++) {
    free(req->mallocs[i].ptr);
  }
  Block_release(req->malloc);
  if (parseBytes > 0)
    Block_release(req->get);
  return;
}

static route_handler_t matchRouteHandler(request_t *req, router_t *router) {
  for (int i = 0; i < router->routeHandlerCount; i++) {
    size_t methodLen = strlen(router->routeHandlers[i].method);
    size_t pathLen = strlen(router->routeHandlers[i].path);
    size_t basePathLen = strlen(router->basePath);

    char *routeHandlerFullPath =
        malloc(sizeof(char) * (basePathLen + pathLen + 1));
    snprintf(routeHandlerFullPath, basePathLen + pathLen + 1, "%s%s",
             router->basePath, router->routeHandlers[i].path);

    if (strncmp(router->routeHandlers[i].method, req->method, methodLen) != 0) {
      free(routeHandlerFullPath);
      continue;
    }

    if (strcmp(routeHandlerFullPath, req->pathMatch) == 0 &&
        router->routeHandlers[i].regex) {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    if (strcmp(routeHandlerFullPath, req->path) == 0) {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    if (strcmp(routeHandlerFullPath, "") == 0 && strcmp(req->path, "/") == 0) {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    free(routeHandlerFullPath);
  }
  return (route_handler_t){.method = NULL, .path = NULL, .handler = NULL};
}

UNUSED static void freeRequest(request_t *req) {
  cleanupHandler *middlewareCleanupBlocks =
      (void (^*)(request_t *))req->middlewareCleanupBlocks;
  for (int i = 0; i < req->middlewareStackCount; i++) {
    middlewareCleanupBlocks[i](req);
    Block_release(middlewareCleanupBlocks[i]);
  }
  free(req->middlewareCleanupBlocks);
  if (req->_method == NULL)
    free((void *)req->method);
  free((void *)req->url);
  free(req->session);
  free((void *)req->ips);
  free((void *)req->subdomains);
  for (int i = 0; i < req->mallocCount; i++) {
    free(req->mallocs[i].ptr);
  }
  for (int i = 0; i < req->blockCopyCount; i++) {
    Block_release(req->blockCopies[i].ptr);
  }
  free((void *)req->path);
  Block_release(req->get);
  Block_release(req->query);
  Block_release(req->params);
  Block_release(req->body);
  Block_release(req->cookie);
  Block_release(req->m);
  Block_release(req->mSet);
  Block_release(req->malloc);
  Block_release(req->blockCopy);
  curl_easy_cleanup(req->curl);
  free((void *)req->queryString);
  free(req);
}

static void freeResponse(response_t *res) {
  Block_release(res->send);
  Block_release(res->sendFile);
  Block_release(res->sendf);
  Block_release(res->sendStatus);
  Block_release(res->set);
  Block_release(res->get);
  Block_release(res->cookie);
  Block_release(res->location);
  Block_release(res->redirect);
  Block_release(res->download);
  Block_release(res->type);
  Block_release(res->json);
  free(res);
}

static void closeClientConnection(client_t client) {
  shutdown(client.socket, SHUT_RDWR);
  close(client.socket);
}

static void buildResponse(client_t client, request_t *req, response_t *res) {
  res->status = 200;
  res->send = resSendFactory(client, res);
  res->sendf = resSendfFactory(res);
  res->sendFile = resSendFileFactory(client, req, res);
  res->sendStatus = resSendStatusFactory(res);
  res->set = resSetFactory(res);
  res->get = resGetFactory(res);
  res->cookie = resCookieFactory(res);
  res->location = resLocationFactory(req, res);
  res->redirect = resRedirectFactory(req, res);
  res->type = resTypeFactory(res);
  res->json = resJsonFactory(res);
  res->download = resDownloadFactory(req, res);
  res->didSend = 0;
}

#ifdef __linux__
/*

A multi-threaded implementation of the client handler that uses epoll().

*/

#define THREAD_NUM 4
#define MAX_EVENTS 64

typedef enum req_status_t { READING, ENDED } req_status_t;

typedef struct http_status_t {
  client_t client;
  req_status_t reqStatus;
  dispatch_source_t timerSource;
} http_status_t;

typedef struct client_thread_args_t {
  int epollFd;
  server_t *server;
  router_t *baseRouter;
} client_thread_args_t;

void *clientAcceptEventHandler(void *args) {
  client_thread_args_t *clientThreadArgs = (client_thread_args_t *)args;

  int epollFd = clientThreadArgs->epollFd;
  server_t *server = clientThreadArgs->server;
  router_t *baseRouter = clientThreadArgs->baseRouter;

  struct epoll_event *events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
  struct epoll_event ev;
  int nfds;

  while (1) {
    // next:
    nfds = epoll_wait(epollFd, events, MAX_EVENTS, -1);

    if (nfds <= 0) {
      log_err("epoll_wait() failed");
      continue;
    }

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == server->socket) {
        while (1) {
          client_t client = acceptClientConnection(server);

          if (client.socket < 0) {
            if (errno == EAGAIN | errno == EWOULDBLOCK) {
              break;
            } else {
              // log_err("accept() failed");
              break;
            }
          }

          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

          http_status_t *status = malloc(sizeof(http_status_t));
          status->client = client;
          status->reqStatus = READING;

          ev.data.ptr = status;

          dispatch_source_t timerSource = dispatch_source_create(
              DISPATCH_SOURCE_TYPE_TIMER, 0, 0, server->serverQueue);

          status->timerSource = timerSource;

          dispatch_time_t delay = dispatch_time(
              DISPATCH_TIME_NOW, ACCEPT_TIMEOUT_SECS * NSEC_PER_SEC);
          dispatch_source_set_timer(timerSource, delay, delay, 0);
          dispatch_source_set_event_handler(timerSource, ^{
            log_err("timeout");
            dispatch_source_cancel(timerSource);
            dispatch_release(timerSource);
            closeClientConnection(client);
            free(status);
          });
          dispatch_resume(timerSource);

          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, client.socket, &ev) < 0) {
            log_err("epoll_ctl() failed");
            free(status);
            continue;
          }
        }
      } else {
        http_status_t *status = (http_status_t *)events[n].data.ptr;
        if (status->reqStatus == READING) {
          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
          ev.data.ptr = status;

          dispatch_source_cancel(status->timerSource);
          dispatch_release(status->timerSource);

          client_t client = status->client;

          request_t *req = malloc(sizeof(request_t));
          buildRequest(req, client, baseRouter);

          if (req->method == NULL) {
            free(status);
            free(req);
            closeClientConnection(client);
            continue;
          }

          response_t *res = malloc(sizeof(response_t));
          buildResponse(client, req, res);

          baseRouter->handler(req, res);

          status->reqStatus = ENDED;

          if (epoll_ctl(epollFd, EPOLL_CTL_MOD, client.socket, &ev) < 0) {
            log_err("epoll_ctl() failed");
            freeResponse(res);
            freeRequest(req);
            closeClientConnection(client);
            continue;
          }

          freeResponse(res);
          freeRequest(req);
          closeClientConnection(client);
        } else if (status->reqStatus == ENDED) {
          free(status);
        }
      }
    }
  }

  free(events);
}

static int initClientAcceptEventHandler(server_t *server,
                                        router_t *baseRouter) {

  pthread_t threads[THREAD_NUM];

  int epollFd = epoll_create1(0);
  check(epollFd >= 0, "epoll_create1() failed");

  client_thread_args_t threadArgs;
  threadArgs.epollFd = epollFd;
  threadArgs.server = server;
  threadArgs.baseRouter = baseRouter;

  struct epoll_event epollEvent;
  epollEvent.events = EPOLLIN | EPOLLET;
  epollEvent.data.fd = server->socket;

  check(epoll_ctl(epollFd, EPOLL_CTL_ADD, server->socket, &epollEvent) >= 0,
        "epoll_ctl() failed");

  for (int i = 0; i < THREAD_NUM; ++i) {
    check(pthread_create(&threads[i], NULL, clientAcceptEventHandler,
                         &threadArgs) >= 0,
          "pthread_create() failed");
  }
  clientAcceptEventHandler(&threadArgs);

  return 0;
error:
  return -1;
}
#elif __MACH__
/*

A concurrent, multi-threaded implementation of the client handler that uses
dispatch_sources.

For an unknown reason this implementation is not working on linux.

*/
static int initClientAcceptEventHandler(server_t *server,
                                        router_t *baseRouter) {
  dispatch_source_t acceptSource = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_READ, server->socket, 0, server->serverQueue);

  dispatch_source_set_event_handler(acceptSource, ^{
    const unsigned long numPendingConnections =
        dispatch_source_get_data(acceptSource);
    for (unsigned long i = 0; i < numPendingConnections; i++) {
      client_t client = acceptClientConnection(server);
      if (client.socket < 0)
        continue;

      dispatch_source_t timerSource = dispatch_source_create(
          DISPATCH_SOURCE_TYPE_TIMER, 0, 0, server->serverQueue);
      dispatch_source_t readSource = dispatch_source_create(
          DISPATCH_SOURCE_TYPE_READ, client.socket, 0, server->serverQueue);

      dispatch_time_t delay =
          dispatch_time(DISPATCH_TIME_NOW, ACCEPT_TIMEOUT_SECS * NSEC_PER_SEC);
      dispatch_source_set_timer(timerSource, delay, delay, 0);
      dispatch_source_set_event_handler(timerSource, ^{
        log_err("timeout");
        dispatch_source_cancel(timerSource);
        dispatch_release(timerSource);
        closeClientConnection(client);
        dispatch_source_cancel(readSource);
        dispatch_release(readSource);
      });

      dispatch_source_set_event_handler(readSource, ^{
        dispatch_source_cancel(timerSource);
        dispatch_release(timerSource);

        request_t *req = malloc(sizeof(request_t));
        buildRequest(req, client, baseRouter);

        if (req->method == NULL) {
          free(req);
          closeClientConnection(client);
          dispatch_source_cancel(readSource);
          dispatch_release(readSource);
          return;
        }

        response_t *res = malloc(sizeof(response_t));
        buildResponse(client, req, res);

        baseRouter->handler(req, res);

        closeClientConnection(client);
        freeResponse(res);
        freeRequest(req);
        dispatch_source_cancel(readSource);
        dispatch_release(readSource);
      });

      dispatch_resume(timerSource);
      dispatch_resume(readSource);
    }
  });
  dispatch_resume(acceptSource);

  return 0;
}
#endif

/* Public functions */

char *matchEmbeddedFile(const char *path, embedded_files_data_t embeddedFiles) {
  size_t pathLen = strlen(path);
  for (int i = 0; i < embeddedFiles.count; i++) {
    int match = 1;
    size_t nameLen = strlen(embeddedFiles.names[i]);
    if (pathLen != nameLen)
      continue;
    for (size_t j = 0; j < nameLen; j++) {
      if (embeddedFiles.names[i][j] != path[j] &&
          embeddedFiles.names[i][j] != '_') {
        match = 0;
      }
    }
    if (match) {
      char *data = malloc(sizeof(char) * (embeddedFiles.lengths[i] + 1));
      memcpy(data, embeddedFiles.data[i], embeddedFiles.lengths[i]);
      data[embeddedFiles.lengths[i]] = '\0';
      return data;
    }
  }
  return (char *)NULL;
};

char *cwdFullPath(const char *path) {
  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  size_t fullPathLen = strlen(cwd) + strlen(path) + 2;
  char *fullPath = malloc(sizeof(char) * fullPathLen);
  snprintf(fullPath, fullPathLen, "%s/%s", cwd, path);
  return fullPath;
}

int writePid(char *pidFile) {
  char buf[100];
  int fd = open(pidFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  snprintf(buf, 100, "%ld\n", (long)getpid());
  return (unsigned long)write(fd, buf, strlen(buf)) == (unsigned long)strlen;
}

unsigned long readPid(char *pidFile) {
  int fd = open(pidFile, O_RDONLY);
  if (fd < 0)
    return -1;
  char buf[100];
  unsigned long pid = 0;
  while (read(fd, buf, 1) > 0) {
    pid = pid * 10 + (buf[0] - '0');
  }
  return pid;
}

char *generateUuid() {
  char *guid = malloc(sizeof(char) * 37);
  if (guid == NULL)
    return NULL;
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, guid);
  return guid;
}

/* Public middleware */

middlewareHandler expressStatic(const char *path, const char *fullPath,
                                embedded_files_data_t embeddedFiles) {
  return Block_copy(^(request_t *req, response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    if (embeddedFiles.count > 0) {
      const char *reqPath = req->path;
      reqPath++;
      char *data = matchEmbeddedFile(reqPath, embeddedFiles);
      const char *mimetype = getMegaMimeType(req->path);
      if (mimetype != NULL)
        res->set("Content-Type", mimetype);
      if (data != NULL) {
        res->send(data);
        free(data);
      } else {
        next();
      }
      return;
    }

    char *filePath = matchFilepath(req, path);

    char *rPath = realpath(filePath, NULL);
    int isTraversal = rPath && strncmp(rPath, fullPath, strlen(fullPath)) != 0;

    if (rPath)
      free(rPath);

    if (isTraversal) {
      if (filePath != NULL)
        free(filePath);
      res->status = 403;
      res->sendf(errorHTML, req->path);
      return;
    }

    if (filePath != NULL) {
      res->sendFile(filePath);
      free(filePath);
    } else {
      next();
    }
  });
}

middlewareHandler
memSessionMiddlewareFactory(mem_session_t *memSession,
                            dispatch_queue_t memSessionQueue) {
  return Block_copy(^(request_t *req, response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    req->session->uuid = req->cookie("sessionUuid");
    if (req->session->uuid == NULL) {
      req->session->uuid = generateUuid();
      cookie_opts_t opts = {
          .path = "/", .maxAge = 60 * 60 * 24 * 365, .httpOnly = true};
      res->cookie("sessionUuid", req->session->uuid, opts);
    }

    int storeExists = 0;
    for (int i = 0; i < memSession->count; i++) {
      if (strcmp(memSession->stores[i]->uuid, req->session->uuid) == 0) {
        storeExists = 1;
        req->session->store = memSession->stores[i]->sessionStore;
        break;
      }
    }

    if (!storeExists) {
      mem_session_store_t *sessionStore = malloc(sizeof(mem_session_store_t));
      sessionStore->count = 0;
      req->session->store = sessionStore;
      mem_store_t *store = malloc(sizeof(mem_store_t));
      store->uuid = (char *)req->session->uuid;
      store->sessionStore = sessionStore;
      dispatch_sync(memSessionQueue, ^{
        memSession->stores[memSession->count++] = store;
      });
    }

    req->session->get = ^(const char *key) {
      mem_session_store_t *sessionStore = req->session->store;
      for (int i = 0; i < sessionStore->count; i++) {
        if (strcmp(sessionStore->keyValues[i].key, key) == 0)
          return (void *)sessionStore->keyValues[i].value;
      }
      return NULL;
    };

    req->session->set = ^(const char *key, void *value) {
      mem_session_store_t *store = req->session->store;
      for (int i = 0; i < store->count; i++) {
        if (strcmp(store->keyValues[i].key, key) == 0) {
          store->keyValues[i].value = value;
          return;
        }
      }
      store->keyValues[store->count].key = key;
      store->keyValues[store->count].value = value;
      store->count++;
    };

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    next();
  });
}

/* expressRouter */

router_t *expressRouter() {
  __block router_t *router = malloc(sizeof(router_t));

  router->basePath = NULL;
  router->isBaseRouter = 0;
  router->routeHandlers = malloc(sizeof(route_handler_t));
  router->routeHandlerCount = 0;
  router->middlewares = malloc(sizeof(middleware_t));
  router->middlewareCount = 0;
  router->routers = malloc(sizeof(router_t *));
  router->routerCount = 0;
  router->appCleanupBlocks = malloc(sizeof(appCleanupHandler));
  router->appCleanupCount = 0;

  int (^isBaseRouter)(void) = ^{
    return router->isBaseRouter;
  };

  void (^addRouteHandler)(const char *, const char *, requestHandler) =
      ^(const char *method, const char *path, requestHandler handler) {
        route_handler_t routeHandler = {
            .method = method,
            .path = !isBaseRouter() && strcmp(path, "/") == 0 ? "" : path,
            .regex = strchr(path, ':') != NULL,
            .handler = handler,
        };

        router->routeHandlers =
            realloc(router->routeHandlers,
                    sizeof(route_handler_t) * (router->routeHandlerCount + 1));

        if (isBaseRouter()) {
          routeHandler.basePath = (char *)router->basePath;
          routeHandler.paramMatch =
              routeHandler.regex ? paramMatch(router->basePath, path) : NULL;
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
    router->middlewares =
        realloc(router->middlewares,
                sizeof(middleware_t) * (router->middlewareCount + 1));
    router->middlewares[router->middlewareCount++] =
        (middleware_t){.handler = handler};
  });

  router->mountTo = Block_copy(^(router_t *baseRouter) {
    if (!baseRouter->basePath)
      return;

    if (strlen(baseRouter->basePath) == 0 &&
        strcmp(router->mountPath, "/") == 0) {
      router->basePath = strdup("");
    } else {
      size_t basePathLen =
          strlen(baseRouter->basePath) + strlen(router->mountPath) + 1;
      router->basePath = malloc(basePathLen);
      snprintf((char *)router->basePath, basePathLen, "%s%s",
               baseRouter->basePath, router->mountPath);
    }

    for (int i = 0; i < router->routeHandlerCount; i++) {
      router->routeHandlers[i].basePath = (char *)router->basePath;
      router->routeHandlers[i].regex = router->routeHandlers[i].regex ||
                                       strchr(router->basePath, ':') != NULL;
      if (router->routeHandlers[i].regex)
        router->routeHandlers[i].paramMatch =
            paramMatch(router->basePath, router->routeHandlers[i].path);
    }

    for (int i = 0; i < router->routerCount; i++) {
      router->routers[i]->mountTo(router);
    }
  });

  router->useRouter = Block_copy(^(char *mountPath, router_t *routerToMount) {
    routerToMount->mountPath = mountPath;
    router->routers = realloc(router->routers,
                              sizeof(router_t *) * (router->routerCount + 1));
    router->routers[router->routerCount++] = routerToMount;
    routerToMount->mountTo(router);
  });

  router->handler = Block_copy(^(request_t *req, response_t *res) {
    // TODO: only run middleware if the request is for this router
    runMiddleware(0, req, res, router, ^{
      route_handler_t routeHandler = matchRouteHandler(req, router);
      if (routeHandler.handler != NULL) {
        req->baseUrl = routeHandler.basePath;
        req->route = (void *)&routeHandler;
        routeHandler.handler(req, res);
        return;
      }
    });

    for (int i = 0; i < router->routerCount; i++) {
      router->routers[i]->handler(req, res);
    }

    if (isBaseRouter() && res->didSend == 0) {
      res->status = 404;
      res->sendf(errorHTML, req->path);
    }
  });

  router->cleanup = Block_copy(^(appCleanupHandler handler) {
    router->appCleanupBlocks =
        realloc(router->appCleanupBlocks,
                sizeof(appCleanupHandler) * (router->appCleanupCount + 1));
    router->appCleanupBlocks[router->appCleanupCount++] = handler;
  });

  router->free = Block_copy(^(void) {
    /* Free route handlers */
    for (int i = 0; i < router->routeHandlerCount; i++) {
      if (router->routeHandlers[i].regex) {
        paramMatchFree(router->routeHandlers[i].paramMatch);
        free(router->routeHandlers[i].paramMatch);
      }
      Block_release(router->routeHandlers[i].handler);
    }
    free(router->routeHandlers);

    /* Free middleware */
    for (int i = 0; i < router->middlewareCount; i++) {
      free((void *)router->middlewares[i].path);
      Block_release(router->middlewares[i].handler);
    }
    free(router->middlewares);

    /* Free routers */
    for (int i = 0; i < router->routerCount; i++) {
      router->routers[i]->free();
      free((void *)router->routers[i]->basePath);
      free(router->routers[i]);
    }
    free(router->routers);

    /* Free app cleanup blocks */
    for (int i = 0; i < router->appCleanupCount; i++) {
      router->appCleanupBlocks[i]();
      Block_release(router->appCleanupBlocks[i]);
    }
    free(router->appCleanupBlocks);

    /* Free router */
    Block_release(router->get);
    Block_release(router->post);
    Block_release(router->put);
    Block_release(router->patch);
    Block_release(router->delete);
    Block_release(router->use);
    Block_release(router->mountTo);
    Block_release(router->useRouter);
    Block_release(router->handler);
    Block_release(router->cleanup);
    Block_release(router->free);
  });

  return router;
}

/* server */

static server_t *expressServer() {
  server_t *server = malloc(sizeof(server_t));

  server->socket = -1;
  server->serverQueue =
      dispatch_queue_create("serverQueue", DISPATCH_QUEUE_CONCURRENT);

  server->close = Block_copy(^() {
    close(server->socket);
    dispatch_release(server->serverQueue);
    Block_release(server->close);
  });

  return server;
}

/* express */

app_t express() {
  app_t app;

  server_t *server = expressServer();
  router_t *baseRouter = expressRouter();

  baseRouter->basePath = "";
  baseRouter->mountPath = "";
  baseRouter->isBaseRouter = 1;

  app.get = baseRouter->get;
  app.post = baseRouter->post;
  app.put = baseRouter->put;
  app.patch = baseRouter->patch;
  app.delete = baseRouter->delete;
  app.use = baseRouter->use;
  app.useRouter = baseRouter->useRouter;
  app.cleanup = baseRouter->cleanup;
  app.server = server;

  app.closeServer = Block_copy(^() {
    printf("\nClosing server...\n");

    baseRouter->free();
    free(baseRouter);

    server->close();
    free(server);
  });

  app.listen = Block_copy(^(int port, void (^callback)()) {
    check(initServerSocket(server) >= 0, "Failed to initialize server socket");
    check(initServerListen(port, server) >= 0, "Failed to listen on port %d",
          port);
    dispatch_async(server->serverQueue, ^{
      check(initClientAcceptEventHandler(server, baseRouter),
            "Failed to initialize client accept event handler");
    error:
      return;
    });
    callback();
    if (port > 0)
      dispatch_main();
  error:
    baseRouter->free();
    free(baseRouter);

    return;
  });

  return app;
};
