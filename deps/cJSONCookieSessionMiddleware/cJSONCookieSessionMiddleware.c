#include <stdlib.h>
#include <stdio.h>
#include <Block.h>
#include <cJSON/cJSON.h>
#include "cJSONCookieSessionMiddleware.h"

middlewareHandler cJSONCookieSessionMiddlewareFactory()
{
  return Block_copy(^(request_t *req, response_t *res, void (^next)(), void (^cleanup)(cleanupHandler)) {
    char *sessionUuid = req->cookie("sessionUuid");
    if (sessionUuid == NULL)
    {
      sessionUuid = generateUuid();
      cookie_opts_t opts = {.path = "/", .maxAge = 60 * 60 * 24 * 365, .httpOnly = 1};
      res->cookie("sessionUuid", sessionUuid, opts);
    }
    req->session->uuid = sessionUuid;

    if (req->cookie("sessionStore"))
    {
      char *sessionStore = req->cookie("sessionStore");
      cJSON *store = cJSON_Parse(sessionStore);
      if (store == NULL)
      {
        req->session->store = cJSON_CreateObject();
      }
      else
      {
        req->session->store = store;
      }
    }
    else
    {
      req->session->store = cJSON_CreateObject();
      res->cookie("sessionStore", cJSON_Print(req->session->store), (cookie_opts_t){.path = "/", .maxAge = 60 * 60 * 24 * 365, .httpOnly = 1});
    }

    req->session->get = ^(char *key) {
      return (void *)cJSON_GetObjectItem(req->session->store, key);
    };

    req->session->set = ^(char *key, void *value) {
      cJSON *item = cJSON_GetObjectItem(req->session->store, key);
      if (item == NULL)
      {
        cJSON_AddItemToObject(req->session->store, key, value);
        cJSON_Delete(item);
      }
      else
      {
        cJSON_ReplaceItemInObject(req->session->store, key, value);
      }
      char *sessionStoreString = cJSON_PrintUnformatted(req->session->store);
      res->cookie("sessionStore", sessionStoreString, (cookie_opts_t){.path = "/", .maxAge = 60 * 60 * 24 * 365, .httpOnly = 1});
    };

    cleanup(Block_copy(^{
      printf("Cleanup cJSONCookieSessionMiddleware\n");
      // cJSON_Delete(req->session->store);
      // Block_release(req->session->get);
      // Block_release(req->session->set);
    }));

    next();
  });
}
