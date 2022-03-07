#include "island.h"

model_t *IslandModel(memory_manager_t *memoryManager) {
  model_t *Island = CreateModel("islands", memoryManager);

  return Island;
}
