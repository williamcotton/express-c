#include "resource-middleware.h"

middlewareHandler resourceMiddleware(resource_library_t *resourceLibrary,
                                     database_pool_t *db) {

  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    model_store_t *modelStore = createModelStore(req->memoryManager);
    resource_store_t *resourceStore = createResourceStore(req->memoryManager);

    for (int i = 0; i < resourceLibrary->count; i++) {
      resource_library_item_t *item = resourceLibrary->items[i];
      model_t *Model = item->ModelFunction(req->memoryManager, db, modelStore);
      resource_t *Resource = item->ResourceFunction(Model, resourceStore);
      Resource->context = req;
      req->mSet(item->name, Resource);
    }

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){

    }));

    next();
  });
}
