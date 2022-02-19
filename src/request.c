#include "express.h"

// TODO: request->trash() ? push and struct with a free() and call them at end?
// TODO: request->free()

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

static mallocBlock reqMallocFactory(request_t *req) {
  req->mallocCount = 0;
  return Block_copy(^(size_t size) {
    void *ptr = malloc(size);
    req->mallocs[req->mallocCount++] = (req_malloc_t){.ptr = ptr};
    return ptr;
  });
}

static copyBlock reqBlockCopyFactory(request_t *req) {
  req->blockCopyCount = 0;
  return Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    req->blockCopies[req->blockCopyCount++] = (req_block_copy_t){.ptr = ptr};
    return ptr;
  });
}

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
  req->paramKeyValueCount = 0;
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
    for (int j = req->cookiesKeyValueCount - 1; j >= 0; j--) {
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

static getMiddlewareBlock reqMiddlewareFactory(request_t *req) {
  return Block_copy(^(const char *key) {
    for (size_t i = 0; i < req->middlewareKeyValueCount; i++) {
      if (strcmp(req->middlewareKeyValues[i].key, key) == 0)
        return req->middlewareKeyValues[i].value;
    }
    return NULL;
  });
}

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

void buildRequest(request_t *req, client_t client, router_t *baseRouter) {
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

void freeRequest(request_t *req) {
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
