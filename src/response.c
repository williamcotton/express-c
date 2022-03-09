#include "express.h"

char *getStatusMessage(int status);

error_t *error404(request_t *req) {
  size_t errorMessageLen =
      strlen(req->path) + strlen(req->method) + strlen("Cannot  ") + 1;
  char *errorMessage = req->malloc(sizeof(char) * errorMessageLen);
  snprintf(errorMessage, errorMessageLen, "Cannot %s %s", req->method,
           req->path);
  error_t *err = req->malloc(sizeof(error_t));
  err->status = 404;
  err->message = errorMessage;
  return err;
}

static size_t getFileSize(UNUSED const char *filePath) {
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
      error_t *err = error404(req);
      res->error(err);
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
    size_t read;
    while ((read = fread(buffer, 1, 4096, file)) > 0) {
      write(client.socket, buffer, read);
    }
    free(buffer);
    free(response);
    fclose(file);
  });
}

static sendStatusBlock resSendStatusFactory(response_t *res) {
  return Block_copy(^(int status) {
    res->status = status;
    res->send(getStatusMessage(status));
  });
}

static sendBlock resTypeFactory(response_t *res) {
  return Block_copy(^(const char *type) {
    const char *mimetype = getMegaMimeType(type);
    if (mimetype != NULL)
      res->set("Content-Type", mimetype);
  });
}

static sendBlock resJsonFactory(response_t *res) {
  return Block_copy(^(const char *json) {
    res->set("Content-Type", "application/json");
    res->send(json);
  });
}

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

static clearCookieBlock resClearCookieFactory(response_t *res) {
  return Block_copy(^(const char *key, cookie_opts_t opts) {
    res->cookie(key, "; expires=Thu, 01 Jan 1970 00:00:00 GMT", opts);
  });
}

static sendBlock resLocationFactory(request_t *req, response_t *res) {
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

static sendBlock resRedirectFactory(UNUSED request_t *req, response_t *res) {
  return Block_copy(^(const char *url) {
    res->status = 302;
    res->location(url);
    res->sendf("Redirecting to %s", url);
    return;
  });
}

static setError resErrorFactory(response_t *res) {
  res->err = NULL;
  return Block_copy(^(error_t *err) {
    res->err = err;
  });
}

static setSenderBlock resSetSenderFactory(request_t *req, response_t *res) {
  return Block_copy(^(const char *key, responseSenderCallback callback) {
    response_sender_t *sender = req->malloc(sizeof(response_sender_t));
    sender->key = key;
    sender->callback = callback;
    res->senders[res->sendersCount++] = sender;
  });
}

static senderBlock resSenderFactory(response_t *res) {
  return Block_copy(^(const char *key, void *value) {
    for (int i = 0; i < res->sendersCount; i++) {
      if (strcmp(res->senders[i]->key, key) == 0) {
        res->senders[i]->callback(res, value);
        return;
      }
    }
    log_err("No sender found for key %s", key);
  });
}

void freeResponse(response_t *res) {
  Block_release(res->send);
  Block_release(res->sendFile);
  Block_release(res->sendf);
  Block_release(res->sendStatus);
  Block_release(res->set);
  Block_release(res->get);
  Block_release(res->cookie);
  Block_release(res->clearCookie);
  Block_release(res->location);
  Block_release(res->redirect);
  Block_release(res->download);
  Block_release(res->type);
  Block_release(res->json);
  Block_release(res->error);
  Block_release(res->sSet);
  Block_release(res->s);
  free(res);
}

void buildResponse(client_t client, request_t *req, response_t *res) {
  res->status = 200;
  res->send = resSendFactory(client, res);
  res->sendf = resSendfFactory(res);
  res->sendFile = resSendFileFactory(client, req, res);
  res->sendStatus = resSendStatusFactory(res);
  res->set = resSetFactory(res);
  res->get = resGetFactory(res);
  res->cookie = resCookieFactory(res);
  res->clearCookie = resClearCookieFactory(res);
  res->location = resLocationFactory(req, res);
  res->redirect = resRedirectFactory(req, res);
  res->type = resTypeFactory(res);
  res->json = resJsonFactory(res);
  res->download = resDownloadFactory(req, res);
  res->error = resErrorFactory(res);
  res->didSend = 0;
  res->sendersCount = 0;
  res->s = resSenderFactory(res);
  res->sSet = resSetSenderFactory(req, res);
}
