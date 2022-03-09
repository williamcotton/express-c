#include "resource-middleware.h"

middlewareHandler
resourceMiddleware(UNUSED resource_library_t *resourceLibrary) {

  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    pg_t *pg = req->m("pg");

    for (int i = 0; i < resourceLibrary->count; i++) {
      resource_library_item_t *item = resourceLibrary->items[i];
      item->Model->setPg(pg);
      item->Model->setInstanceMemoryManager(req->memoryManager);
      req->mSet(item->name, item->Resource);
    }

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){

    }));

    next();
  });
}
