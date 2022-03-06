#include "island.h"

model_t *IslandModel(memory_manager_t *memoryManager, pg_t *pg) {
  model_t *Island = CreateModel("islands", memoryManager, pg);

  return Island;
}
