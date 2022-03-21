#include "island.h"

model_t *IslandModel(memory_manager_t *memoryManager, pg_t *pg,
                     model_store_t *modelStore) {
  model_t *Island = CreateModel("islands", memoryManager, pg, modelStore);

  return Island;
}
