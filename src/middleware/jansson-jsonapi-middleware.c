#include "jansson-jsonapi-middleware.h"

#define JSON_API_MIME_TYPE "application/vnd.api+json"

middlewareHandler janssonJsonapiMiddleware() {
  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    char *contentType = (char *)req->get("Content-Type");
    if (contentType != NULL &&
        strncmp(contentType, JSON_API_MIME_TYPE, 25) == 0) {
      debug("Content-Type: %s", contentType);
    };

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    next();
  });
}