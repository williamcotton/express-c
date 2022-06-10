#include "resource.h"

resource_library_t *initResourceLibrary(memory_manager_t *memoryManager,
                                        database_pool_t *db) {
  resource_library_t *library =
      mmMalloc(memoryManager, sizeof(resource_library_t));
  library->count = 0;
  library->modelStore = createModelStore(memoryManager);
  library->resourceStore = createResourceStore(memoryManager);
  library->add = mmBlockCopy(
      memoryManager, ^(const char *name, ModelFunction ModelFunction,
                       ResourceFunction ResourceFunction) {
        resource_library_item_t *item =
            mmMalloc(memoryManager, sizeof(resource_library_item_t));
        item->name = name;
        item->ModelFunction = ModelFunction;
        item->ResourceFunction = ResourceFunction;
        item->model = ModelFunction(memoryManager, db, library->modelStore);
        item->resource = ResourceFunction(item->model, library->resourceStore);
        library->items[library->count++] = item;
      });
  return library;
}
