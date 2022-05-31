#include "resource.h"

resource_library_t *initResourceLibrary(memory_manager_t *memoryManager) {
  resource_library_t *library =
      mmMalloc(memoryManager, sizeof(resource_library_t));
  library->count = 0;
  library->add = mmBlockCopy(
      memoryManager, ^(const char *name, ModelFunction ModelFunction,
                       ResourceFunction ResourceFunction) {
        resource_library_item_t *item =
            mmMalloc(memoryManager, sizeof(resource_library_item_t));
        item->name = name;
        item->ModelFunction = ModelFunction;
        item->ResourceFunction = ResourceFunction;
        library->items[library->count++] = item;
      });
  return library;
}
