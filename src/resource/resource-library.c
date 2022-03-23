#include "resource.h"

resource_library_t *initResourceLibrary(memory_manager_t *memoryManager) {
  resource_library_t *library =
      memoryManager->malloc(sizeof(resource_library_t));
  library->count = 0;
  library->add =
      memoryManager->blockCopy(^(const char *name, ModelFunction ModelFunction,
                                 ResourceFunction ResourceFunction) {
        resource_library_item_t *item =
            memoryManager->malloc(sizeof(resource_library_item_t));
        item->name = name;
        item->ModelFunction = ModelFunction;
        item->ResourceFunction = ResourceFunction;
        library->items[library->count++] = item;
      });
  return library;
}
