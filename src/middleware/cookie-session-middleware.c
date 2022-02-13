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

#include "cookie-session-middleware.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>

middlewareHandler cookieSessionMiddlewareFactory() {
  return Block_copy(^(request_t *req, response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    if (req->cookie("sessionUuid") == NULL) {
      char *sessionUuid = generateUuid();
      cookie_opts_t opts = {
          .path = "/", .maxAge = 60 * 60 * 24 * 365, .httpOnly = 1};
      res->cookie("sessionUuid", sessionUuid, opts);
      free(sessionUuid);
    }
    req->session->uuid = req->cookie("sessionUuid");

    if (req->cookie("sessionStore")) {
      const char *sessionStore = req->cookie("sessionStore");
      cJSON *store = cJSON_Parse(sessionStore);
      if (store == NULL)
        req->session->store = cJSON_CreateObject();
      else
        req->session->store = store;
    } else {
      req->session->store = cJSON_CreateObject();
      char *sessionStoreString = cJSON_Print(req->session->store);
      res->cookie("sessionStore", sessionStoreString,
                  (cookie_opts_t){.path = "/",
                                  .maxAge = 60 * 60 * 24 * 365,
                                  .httpOnly = 1});
      free(sessionStoreString);
    }

    req->session->get = ^(const char *key) {
      return (void *)cJSON_GetObjectItem(req->session->store, key);
    };

    req->session->set = ^(const char *key, void *value) {
      cJSON *item = cJSON_GetObjectItem(req->session->store, key);
      if (item == NULL) {
        cJSON_AddItemToObject(req->session->store, key, value);
        cJSON_Delete(item);
      } else {
        cJSON_ReplaceItemInObject(req->session->store, key, value);
      }
      char *sessionStoreString = cJSON_PrintUnformatted(req->session->store);
      res->cookie("sessionStore", sessionStoreString,
                  (cookie_opts_t){.path = "/",
                                  .maxAge = 60 * 60 * 24 * 365,
                                  .httpOnly = 1});
      free(sessionStoreString);
    };

    cleanup(Block_copy(^(UNUSED request_t *finishedReq) {
      cJSON_Delete(finishedReq->session->store);
    }));

    next();
  });
}
