#include "resource.h"

resource_library_t *initResourceLibrary(memory_manager_t *appMemoryManager) {
  resource_library_t *library =
      appMemoryManager->malloc(sizeof(resource_library_t));
  library->count = 0;
  library->add = appMemoryManager->blockCopy(
      ^(const char *name, ModelFunction ModelFunction,
        ResourceFunction ResourceFunction) {
        resource_library_item_t *item =
            appMemoryManager->malloc(sizeof(resource_library_item_t));
        item->name = name;
        item->Model = ModelFunction(appMemoryManager);
        item->Resource = ResourceFunction(item->Model);
        library->items[library->count++] = item;
      });
  return library;
}
