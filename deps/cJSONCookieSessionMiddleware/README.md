# cJSON Cookie Session Middleware

Session middleware built on top of [cJSON](https://github.com/DaveGamble/cJSON) and uses the express-c framework `req->session` and `req->cookie` features to provide a simple cookie-based session management system.

```c
#include "cJSONCookieSessionMiddleware.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
```

### Middleware

The `express-c` framework defines a basic session_t type that is used to store session data.

```c
typedef struct session_t {
  const char *uuid;
  void *store;
  void * (^get)(const char *key);
  void (^set)(const char *key, void *value);
} session_t;
```

The type is defined using void pointers to allow for different session store implementations.

For this implementation we typecasy the `req->session->store` to a cJSON object that we serialize to a cookie.

```c
middlewareHandler cJSONCookieSessionMiddlewareFactory() {
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
```
