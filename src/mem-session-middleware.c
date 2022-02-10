#include "express.h"

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
