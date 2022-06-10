#include "island.h"

model_t *IslandModel(memory_manager_t *memoryManager, database_pool_t *db,
                     model_store_t *modelStore) {
  model_t *Island = CreateModel("islands", memoryManager, db, modelStore);

  Island->attribute("name", "text", NULL);

  return Island;
}
